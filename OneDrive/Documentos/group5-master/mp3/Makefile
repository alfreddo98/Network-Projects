all: reliable_sender reliable_receiver

reliable_sender:
	gcc -g3 -pthread -o reliable_sender sender_main.c -Dsender

reliable_receiver:
	gcc -g3 -pthread -o reliable_receiver receiver_main.c -Dreceiver

clean:
	@rm -f reliable_sender reliable_receiver *.o
