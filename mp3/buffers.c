// Holds the data structures and functions for the send and receive buffers.
// cdgerth2 alfredo7

// Comment out this next line to turn off verbosity of the buffer system:
// #define verbose

// #define sender

// Includes:
#include <time.h>

// Compiler Macros:
#if defined sender
#define BUF_SIZE 80000
#define BURST_SIZE 5
#define MAX_DELAY    (700000 * BURST_SIZE)
#define HIGHER_DELAY (650000 * BURST_SIZE)
#define HIGH_DELAY   (600000 * BURST_SIZE)
#define MED_DELAY    (400000 * BURST_SIZE)
#define LOW_DELAY    (200000 * BURST_SIZE)
#define LOWER_DELAY  (150000 * BURST_SIZE)
#define MIN_DELAY    (100000 * BURST_SIZE)
#endif

#if defined receiver
#define BUF_SIZE 100000
#endif





// #define MAX_SIZE 1468
#define MAX_SIZE 1468
#define YES 1
#define NO 0
#define TRUE 1
#define FALSE 0
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

// Data Structures:
typedef struct payload_t{
	unsigned short length;
	unsigned short seqNum;
	char data[MAX_SIZE];
}payload_t;


typedef struct buffer_node {
    struct timespec expires; // time the packet was sent -- unused for the receive buffer
    struct timespec time_sent; // time the packet was sent -- unused for the receive buffer
    unsigned short seqNum; // Sequence number of the given packet
    unsigned short len; // Length of the data in the payload
    unsigned char processed; // boolean for if the packet has been acked or written to file, depending on which implementation
    payload_t * payload; // Pointer to the dynamically allocated payload
} buffer_node;

static struct buffer_node ring_buffer[BUF_SIZE]; // Define the buffer itself

#if defined sender
    static unsigned short oldest_seq_not_acked;
    static int newest_seq_sent;
    static unsigned long send_delay; // Amount of useconds to wait in between each packet with the aim of keeping the buffer half full
    static unsigned long average_rtt; // Holds the average RTT of packets
    static unsigned long average_throughput; // Holds the throughput in packets / sec that can be handled by the link
    unsigned int fail_count;
    static int gain_count;
    static unsigned int no_drops;
    static char current_speed;
    static float average_fail_count;
    unsigned int resent;
    static struct timespec last_speed_change;
#endif

#if defined receiver
    static unsigned short oldest_seq_not_written;
    static unsigned short newest_seq_received;
    unsigned int duplicates;
#endif

// Function Declarations:

// Call this at the beginning of the program -- resets the buffers.
extern void init_buffers();

// Buffer Access
// Returns a pointer to the buffer node associated with the given sequence number.
// Inputs: sequence number to query
// Output: Pointer to the buffer node
extern buffer_node * buffer_access(unsigned short seqNum);

// Sender only Functions:
#if defined sender

    // Buffer Insert
    // Inserts data to the buffer (data meaning raw data from the file). Assumes data will be immediately sent.
    // Inputs: pointer to the payload with everything filled in except for the sequence number.
    // BIG NOTE: Leave the header space at the beginning of raw data -> ie file data shoould start 4B into the buffer. This way we can reuse it as the payload.
    // Outputs: The sequence number of the newly created node.
    extern int buffer_insert(payload_t * payload);

    // Buffer Mark Acked
    // Marks packet as acknowledged and frees the memory associated with the payload.
    // Inputs: sequence number
    // Outputs: none
    extern void buffer_mark_acked(unsigned short seqNum);

    // Buffer To be Resent
    // Returns a pointer to the node of a packet that should be resent if one exists, null if otherwise
    // Inputs: none
    // Outpus: pointer to the payload that should be resent.
    extern payload_t * buffer_to_be_resent();

    // returns the current send delay to be implemented in us.
    extern void current_send_delay();

    // returns true if every packet sent has been acked or false otherwise
    extern unsigned char sender_done();

#endif

// Receiver only Functions:
#if defined receiver

    // Buffer Insert
    // Inserts data to the buffer (data meaning raw payload from the internet)
    // Inputs: pointer to the internet data and the number of bytes of this data
    // Outputs: The sequence number of the newly created node. (or -1 if buffer overflow)
    extern int buffer_insert(payload_t * payload);

    // Buffer To be Written
    // Returns a pointer to the node of a packet that is ready to be written to file, if it exists.
    // Inputs: none
    // Outputs: pointer to the buffer node to be written to file.
    extern payload_t * buffer_to_be_written();

    // Buffer Mark Written
    // Once the data is written to the file, call this to free up the memory.
    // Inputs: sequence number of node to be freed
    // Outputs: none
    extern void buffer_mark_written(unsigned short seqNum);

    unsigned char receiver_done();

    extern short int seqs_done();


#endif










extern void init_buffers(){

    int i;
    
    #if defined sender
        oldest_seq_not_acked = 0;
        newest_seq_sent = -1;
        average_rtt = 4000000; // 40 ms
        average_throughput = 13000000; //1000 Mbps
        fail_count = 0;
        gain_count = 0;
        current_speed = 4;
        average_fail_count = 1;
        no_drops = 0;
        resent = 0;
        clock_gettime(0, &last_speed_change);
    #endif

    #if defined receiver
        oldest_seq_not_written = 0;
        newest_seq_received = 0;
        duplicates = 0;
    #endif

    for (i = 0; i < BUF_SIZE; i++){
        ring_buffer[i].len = 0;
        ring_buffer[i].payload = NULL;
        ring_buffer[i].processed = YES;
        ring_buffer[i].seqNum = i;
        ring_buffer[i].time_sent.tv_sec = 0;
        ring_buffer[i].time_sent.tv_nsec = 0;
        ring_buffer[i].expires.tv_sec = 0;
        ring_buffer[i].expires.tv_nsec = 0;
    }


}


extern buffer_node * buffer_access(unsigned short seqNum){
   
    #if defined sender
        if (seqNum > newest_seq_sent || seqNum < oldest_seq_not_acked){
            #if defined verbose
                printf("Invalid buffer access: SEQ(%d).\n", seqNum);
            #endif
            return NULL;
        }
    #endif

    #if defined receiver
        if (seqNum > newest_seq_received || seqNum < oldest_seq_not_written){
            #if defined verbose
                printf("Invalid buffer access: SEQ(%d).\n", seqNum);
            #endif
            return NULL;
        }
    #endif

    // Return the actual element:
    return &(ring_buffer[seqNum % BUF_SIZE]);

}


// Sender only Functions:
#if defined sender

    
    extern int buffer_insert(payload_t * payload){

        if ((newest_seq_sent != -1) && ((newest_seq_sent - oldest_seq_not_acked) == (BUF_SIZE - 1))){
            #if defined verbose
                printf("Buffer Overflow (%d) (%lu).\n", newest_seq_sent, average_rtt);
            #endif

            average_throughput -= 1000;

            if (average_throughput < 10000)
                average_throughput = 10000;

            return -1;
        }

        newest_seq_sent++;

        payload->seqNum = newest_seq_sent;

        ring_buffer[newest_seq_sent % BUF_SIZE].processed = NO;
        ring_buffer[newest_seq_sent % BUF_SIZE].len = payload->length;
        ring_buffer[newest_seq_sent % BUF_SIZE].seqNum = newest_seq_sent;
        ring_buffer[newest_seq_sent % BUF_SIZE].payload = payload;
        clock_gettime(0, &ring_buffer[newest_seq_sent % BUF_SIZE].time_sent);
        clock_gettime(0, &ring_buffer[newest_seq_sent % BUF_SIZE].expires);
        ring_buffer[newest_seq_sent % BUF_SIZE].expires.tv_nsec = (ring_buffer[newest_seq_sent % BUF_SIZE].expires.tv_nsec + average_rtt * 2) % 1000000000;
        ring_buffer[newest_seq_sent % BUF_SIZE].expires.tv_sec += (ring_buffer[newest_seq_sent % BUF_SIZE].expires.tv_nsec + average_rtt * 2) / 1000000000;


        return newest_seq_sent;
    }


    extern void buffer_mark_acked(unsigned short seqNum){

        struct timespec now;
        long difference;

        if (seqNum > newest_seq_sent || seqNum < oldest_seq_not_acked){
            #if defined verbose
                printf("Invalid buffer node acked: SEQ(%d). (%lu)\n", seqNum, average_rtt);
            #endif
            return;
        }


        ring_buffer[seqNum % BUF_SIZE].processed = YES;

        // printf("Fails: %d\n", fail_count);

        if (fail_count < 3)
            fail_count = 0;

        if (fail_count == 0)
            no_drops++;
        else
            no_drops = 0;


        average_fail_count = ((float)average_fail_count * (float)0.99) + ((float)fail_count * (float)0.01);

        if (no_drops == 100 && average_fail_count < 0.35){
            struct timespec now;
            clock_gettime(0, &now);
            if (now.tv_sec > last_speed_change.tv_sec + 1){
                current_speed++;
                average_fail_count = 2;
                clock_gettime(0, &last_speed_change);
            }
        }

        if (average_fail_count > 3.5){
            struct timespec now;
            clock_gettime(0, &now);
            if (now.tv_sec > last_speed_change.tv_sec + 1){
                current_speed--;
                average_fail_count = 0.3;
                clock_gettime(0, &last_speed_change);
            }
        }

        if (current_speed < 0)
            current_speed = 0;
        else if (current_speed > 6)
            current_speed = 6;


        fail_count = 0;


        clock_gettime(0, &now);

        difference = ((1000000000 * (now.tv_sec - ring_buffer[seqNum % BUF_SIZE].time_sent.tv_sec)) + (now.tv_nsec - ring_buffer[seqNum % BUF_SIZE].time_sent.tv_nsec)); // calculate the difference in us

        if (difference <= 0)
            difference = average_rtt;

        
        if (((long)difference - (long)10000000) >= (long)average_rtt)
            gain_count--;
        else if (((long)difference + (long)30000000) <= (long)average_rtt)
            gain_count++;

        average_rtt = (float)(0.99) * (float)average_rtt + (float)(0.01) * (float)difference; //make the averge rtt take into acount the past rtt


        free(ring_buffer[seqNum % BUF_SIZE].payload); // free the memory

        ring_buffer[seqNum % BUF_SIZE].payload = NULL; // delete the invalid pointer


        // if this was the oldest record, then increment
        while (ring_buffer[oldest_seq_not_acked % BUF_SIZE].processed 
                && oldest_seq_not_acked < (newest_seq_sent))
            oldest_seq_not_acked++;


    }

  
    extern payload_t * buffer_to_be_resent(){
        
        struct timespec now;
        long difference;

        int i;
        clock_gettime(0, &now);

        for (i=oldest_seq_not_acked; i <= newest_seq_sent; i++){

            difference = ((1000000000 * (now.tv_sec - ring_buffer[i % BUF_SIZE].expires.tv_sec)) + (now.tv_nsec - ring_buffer[i % BUF_SIZE].expires.tv_nsec));

            // if (difference > 5000000000){
            //     printf("Receiver went down.\n");
            //     exit(1);
            // }

            if ((difference > 0) && ring_buffer[i % BUF_SIZE].processed == NO){

                fail_count++;

                #if defined verbose
                    printf("Packet (%d) was lost.\n", i);
                #endif

                if (gain_count < 0)
                    gain_count /= 1.1;

                average_throughput -= 100;

                clock_gettime(0, &ring_buffer[i % BUF_SIZE].time_sent);

                clock_gettime(0, &ring_buffer[i % BUF_SIZE].expires);
                ring_buffer[i % BUF_SIZE].expires.tv_nsec = (ring_buffer[i % BUF_SIZE].expires.tv_nsec + average_rtt * 2) % 1000000000;
                ring_buffer[i % BUF_SIZE].expires.tv_sec += (ring_buffer[i % BUF_SIZE].expires.tv_nsec + average_rtt * 2) / 1000000000;


                resent++;

                return ring_buffer[i % BUF_SIZE].payload;

            }
        
        }
        return NULL;
    }

    extern void current_send_delay(){
        struct timespec wait_time;
        unsigned long delay;
        char speed;

        // if (gain_count > 200)
        //     speed = current_speed - 1;
        // else
        speed = current_speed;


        printf("Gain: %d; RTT: %lu; Loss: %f; Speed: %d\n", gain_count, average_rtt, average_fail_count, current_speed);

        switch(speed){
            case 0:  delay = MAX_DELAY;    break;
            case 1:  delay = HIGHER_DELAY; break;
            case 2:  delay = HIGH_DELAY;   break;
            case 3:  delay = MED_DELAY;    break; 
            case 4:  delay = LOW_DELAY;    break;
            case 5:  delay = LOWER_DELAY;  break;
            case 6:  delay = MIN_DELAY;    break;
            default: delay = MAX_DELAY;    break;
        };

        wait_time.tv_nsec = delay % 10000000000;
        wait_time.tv_sec  = delay / 10000000000;

        #if defined verbose
            printf("PPS: %llu DELAY: %ld WAIT: %lu\n", pps, delay, wait_time.tv_nsec);
        #endif
        nanosleep(&wait_time, NULL);
        return;
    }

    unsigned char sender_done(){
        if (oldest_seq_not_acked == (newest_seq_sent))
            return TRUE;
        else
            return FALSE; 
    }

#endif


#if defined receiver

    extern int buffer_insert(payload_t * payload){
        
        unsigned short seq_num;
        seq_num = payload->seqNum; // pull out the sequence number from the payload

        if (seq_num < oldest_seq_not_written){ // this is a duplicate and we can disregard it
            #if defined verbose
            printf("Duplicate packet (%d).\n", seq_num);
            #endif
            duplicates++;
            return -1;
        }


        if (seq_num - oldest_seq_not_written >= BUF_SIZE){
            #if defined verbose
                printf("Buffer overflow Seq(%d).\n", seq_num);
            #endif
            return -2;
        }

        newest_seq_received = MAX(seq_num, newest_seq_received); // update the newest sequence recieved

        ring_buffer[seq_num % BUF_SIZE].seqNum = seq_num;
        ring_buffer[seq_num % BUF_SIZE].len = payload->length;
        ring_buffer[seq_num % BUF_SIZE].processed = NO;
        ring_buffer[seq_num % BUF_SIZE].payload = payload;

        return seq_num;

    }

    extern payload_t * buffer_to_be_written(){

        if (ring_buffer[oldest_seq_not_written % BUF_SIZE].processed == YES){ // if we have not received it yet then return null
            return NULL;
        }

        return ring_buffer[(oldest_seq_not_written) % BUF_SIZE].payload; // otherwise send back the thing that we can now write.
    }


    extern void buffer_mark_written(unsigned short seqNum){

        if (seqNum != oldest_seq_not_written){ // we should only have written the oldest piece that hasn't yet been put into the file
            #if defined verbose
                printf("Invalid mark written (%d).\n", seqNum);
            #endif
        }

        if (ring_buffer[seqNum % BUF_SIZE].payload) // make sure we are not freeing a null pointer
            free(ring_buffer[seqNum % BUF_SIZE].payload); // free the memory

        ring_buffer[seqNum % BUF_SIZE].processed = YES; // mark as written
        oldest_seq_not_written++; // increment the oldest not written

    }

    extern short int seqs_done(){
        if (oldest_seq_not_written > 0)
            return oldest_seq_not_written - 1;
        else
            return 0;
    }

    extern unsigned char receiver_done(){
        if (oldest_seq_not_written == newest_seq_received)
            return TRUE;
        else
            return FALSE; 
    }

#endif