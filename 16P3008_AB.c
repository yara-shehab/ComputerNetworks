#include <stdio.h>


#define BIDIRECTIONAL 0   
struct msg {
	char data[20];
};


struct pkt {
	int seqnum;
	int acknum;
	int checksum;
	char payload[20];
};



#define TIME_OUT 24.0
#define DEBUG 1

int seq_expect_send;	
int seq_expect_recv;	
int is_waiting;			
struct pkt waiting_packet;	


print_pkt(action, packet)
char *action;
struct pkt packet;
{
	printf("%s: ", action);
	printf("seq = %d, ack = %d, checksum = %x, ", packet.seqnum, packet.acknum, packet.checksum);
	int i;
	for (i = 0; i < 20; i++)
		putchar(packet.payload[i]);
	putchar('\n');
}

/* Compute checksum */
int compute_check_sum(packet)
struct pkt packet;
{
	int sum = 0, i = 0;
	sum = packet.checksum;
	sum += packet.seqnum;
	sum += packet.acknum;
	sum = (sum >> 16) + (sum & 0xffff);
	for (i = 0; i < 20; i += 2) {
		sum += (packet.payload[i] << 8) + packet.payload[i+1];
		sum = (sum >> 16) + (sum & 0xffff);
	}
	sum = (~sum) & 0xffff;
	return sum;
}

/* called from layer 5, passed the data to be sent to other side */
A_output(message)
struct msg message;
{
	/* If A is waiting, ignore the message */
	if (is_waiting)
		return;
	/* Send packet to B side */
	memcpy(waiting_packet.payload, message.data, sizeof(message.data));
	waiting_packet.seqnum = seq_expect_send;
	waiting_packet.checksum = 0;
	waiting_packet.checksum = compute_check_sum(waiting_packet);
	tolayer3(0, waiting_packet);
	starttimer(0, TIME_OUT);
	is_waiting = 1;
	/* Debug output */
	if (DEBUG)
		print_pkt("Sent", waiting_packet);
}

B_output(message) 
struct msg message;
{

}

/* called from layer 3, when a packet arrives for layer 4 */
A_input(packet)
struct pkt packet;
{
	stoptimer(0);
	if (packet.acknum == seq_expect_send) {	/* ACK */
		seq_expect_send = 1 - seq_expect_send;
		is_waiting = 0;
	} else if (packet.acknum == -1) {		/* NAK */
		tolayer3(0, waiting_packet);
		starttimer(0, TIME_OUT);
	}
}

/* called when A's timer goes off */
A_timerinterrupt()
{
	tolayer3(0, waiting_packet);
	starttimer(0, TIME_OUT);
} 

A_init()
{
	seq_expect_send = 0;
	is_waiting = 0;
}


B_input(packet)
struct pkt packet;
{
	if (packet.seqnum == seq_expect_recv) {
		/* If corruption occurs, send NAK */
		if (compute_check_sum(packet)) {
			struct pkt nakpkt;
			nakpkt.acknum = -1;
			tolayer3(1, nakpkt);
			return;
		}
		/* Pass data to layer5 */
		struct msg message;
		memcpy(message.data, packet.payload, sizeof(packet.payload));
		tolayer5(1, message);
		seq_expect_recv = 1 - seq_expect_recv;
		/* Debug output */
		if (DEBUG)
			print_pkt("Accpeted", packet);
	}
	/* Send ACK to A side */
	struct pkt ackpkt;
	ackpkt.acknum = packet.seqnum;
	tolayer3(1, ackpkt);
}


B_timerinterrupt()
{

}


B_init()
{
	seq_expect_recv = 0;
}


struct event {
	float evtime;           /* event time */
	int evtype;             /* event type code */
	int eventity;           /* entity where event occurs */
	struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
	struct event *prev;
	struct event *next;
};
struct event *evlist = NULL;   /* the event list */


#define  TIMER_INTERRUPT 0
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1



int TRACE = 1;             
int nsim = 0;              /* number of messages from 5 to 4 so far */
int nsimmax = 0;           /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

main()
{
	struct event *eventptr;
	struct msg  msg2give;
	struct pkt  pkt2give;

	int i, j;
	char c;

	init();
	A_init();
	B_init();

	while (1) {
		eventptr = evlist;            /* get next event to simulate */
		if (eventptr == NULL)
			goto terminate;
		evlist = evlist->next;        /* remove this event from event list */
		if (evlist != NULL)
			evlist->prev = NULL;
		if (TRACE >= 2) {
			printf("\nEVENT time: %f,", eventptr->evtime);
			printf("  type: %d", eventptr->evtype);
			if (eventptr->evtype == 0)
				printf(", timerinterrupt  ");
			else if (eventptr->evtype == 1)
				printf(", fromlayer5 ");
			else
				printf(", fromlayer3 ");
			printf(" entity: %d\n", eventptr->eventity);
		}
		time = eventptr->evtime;        /* update time to next event time */
		if (nsim == nsimmax)
			break;                        /* all done with simulation */
		if (eventptr->evtype == FROM_LAYER5 ) {
			generate_next_arrival();   /* set up future arrival */
			/* fill in msg to give with string of same letter */
			j = nsim % 26;
			for (i = 0; i < 20; i++)
				msg2give.data[i] = 97 + j;
			if (TRACE > 2) {
				printf("          MAINLOOP: data given to student: ");
				for (i = 0; i < 20; i++)
					printf("%c", msg2give.data[i]);
				printf("\n");
			}
			nsim++;
			if (eventptr->eventity == A)
				A_output(msg2give);
			else
				B_output(msg2give);
		}
		else if (eventptr->evtype ==  FROM_LAYER3) {
			pkt2give.seqnum = eventptr->pktptr->seqnum;
			pkt2give.acknum = eventptr->pktptr->acknum;
			pkt2give.checksum = eventptr->pktptr->checksum;
			for (i = 0; i < 20; i++)
				pkt2give.payload[i] = eventptr->pktptr->payload[i];
			if (eventptr->eventity == A)     /* deliver packet by calling */
				A_input(pkt2give);            /* appropriate entity */
			else
				B_input(pkt2give);
			free(eventptr->pktptr);          /* free the memory for packet */
		}
		else if (eventptr->evtype ==  TIMER_INTERRUPT) {
			if (eventptr->eventity == A)
				A_timerinterrupt();
			else
				B_timerinterrupt();
		}
		else  {
			printf("INTERNAL PANIC: unknown event type \n");
		}
		free(eventptr);
	}

terminate:
	printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n", time, nsim);
}



init()                         /* initialize simulator */
{
	int i;
	float sum, avg;
	float jimsrand();


	printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
	printf("Enter the number of messages to simulate: ");
	scanf("%d", &nsimmax);
	printf("Enter  packet loss probability [enter 0.0 for no loss]:");
	scanf("%f", &lossprob);
	printf("Enter packet corruption probability [0.0 for no corruption]:");
	scanf("%f", &corruptprob);
	printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
	scanf("%f", &lambda);
	printf("Enter TRACE:");
	scanf("%d", &TRACE);

	srand(9999);              /* init random number generator */
	sum = 0.0;                /* test random number generator for students */
	for (i = 0; i < 1000; i++)
		sum = sum + jimsrand(); /* jimsrand() should be uniform in [0,1] */
	avg = sum / 1000.0;
	if (avg < 0.25 || avg > 0.75) {
		printf("It is likely that random number generation on your machine\n" );
		printf("is different from what this emulator expects.  Please take\n");
		printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
		exit(0);
	}

	ntolayer3 = 0;
	nlost = 0;
	ncorrupt = 0;

	time = 0.0;                  /* initialize time to 0.0 */
	generate_next_arrival();     /* initialize event list */
}


float jimsrand()
{
	double mmm = 32767;   	   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
	float x;                   /* individual students may need to change mmm */
	x = rand() / mmm;          /* x should be uniform in [0,1] */
	return (x);
}



generate_next_arrival()
{
	double x, log(), ceil();
	struct event *evptr;
	char *malloc();
	float ttime;
	int tempint;

	if (TRACE > 2)
		printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

	x = lambda * jimsrand() * 2; /* x is uniform on [0,2*lambda] */
	/* having mean of lambda        */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtime =  time + x;
	evptr->evtype =  FROM_LAYER5;
	if (BIDIRECTIONAL && (jimsrand() > 0.5) )
		evptr->eventity = B;
	else
		evptr->eventity = A;
	insertevent(evptr);
}


insertevent(p)
struct event *p;
{
	struct event *q, *qold;

	if (TRACE > 2) {
		printf("            INSERTEVENT: time is %lf\n", time);
		printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
	}
	q = evlist;     /* q points to header of list in which p struct inserted */
	if (q == NULL) { /* list is empty */
		evlist = p;
		p->next = NULL;
		p->prev = NULL;
	}
	else {
		for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next)
			qold = q;
		if (q == NULL) { /* end of list */
			qold->next = p;
			p->prev = qold;
			p->next = NULL;
		}
		else if (q == evlist) { /* front of list */
			p->next = evlist;
			p->prev = NULL;
			p->next->prev = p;
			evlist = p;
		}
		else {     /* middle of list */
			p->next = q;
			p->prev = q->prev;
			q->prev->next = p;
			q->prev = p;
		}
	}
}

printevlist()
{
	struct event *q;
	int i;
	printf("--------------\nEvent List Follows:\n");
	for (q = evlist; q != NULL; q = q->next) {
		printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype, q->eventity);
	}
	printf("--------------\n");
}

stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
	struct event *q, *qold;

	if (TRACE > 2)
		printf("          STOP TIMER: stopping timer at %f\n", time);
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
	for (q = evlist; q != NULL ; q = q->next)
		if ( (q->evtype == TIMER_INTERRUPT  && q->eventity == AorB) ) {
			/* remove this event */
			if (q->next == NULL && q->prev == NULL)
				evlist = NULL;       /* remove first and only event on list */
			else if (q->next == NULL) /* end of list - there is one in front */
				q->prev->next = NULL;
			else if (q == evlist) { /* front of list - there must be event after */
				q->next->prev = NULL;
				evlist = q->next;
			}
			else {     /* middle of list */
				q->next->prev = q->prev;
				q->prev->next =  q->next;
			}
			free(q);
			return;
		}
	printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


starttimer(AorB, increment)
int AorB;  /* A or B is trying to stop timer */
float increment;
{

	struct event *q;
	struct event *evptr;
	char *malloc();

	if (TRACE > 2)
		printf("          START TIMER: starting timer at %f\n", time);

	for (q = evlist; q != NULL ; q = q->next)
		if ( (q->evtype == TIMER_INTERRUPT  && q->eventity == AorB) ) {
			printf("Warning: attempt to start a timer that is already started\n");
			return;
		}

	/* create future event for when timer goes off */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtime =  time + increment;
	evptr->evtype =  TIMER_INTERRUPT;
	evptr->eventity = AorB;
	insertevent(evptr);
}


/************************** TOLAYER3 ***************/
tolayer3(AorB, packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
	struct pkt *mypktptr;
	struct event *evptr, *q;
	char *malloc();
	float lastime, x, jimsrand();
	int i;


	ntolayer3++;

	/* simulate losses: */
	if (jimsrand() < lossprob)  {
		nlost++;
		if (TRACE > 0)
			printf("          TOLAYER3: packet being lost\n");
		return;
	}

	mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
	mypktptr->seqnum = packet.seqnum;
	mypktptr->acknum = packet.acknum;
	mypktptr->checksum = packet.checksum;
	for (i = 0; i < 20; i++)
		mypktptr->payload[i] = packet.payload[i];
	if (TRACE > 2)  {
		printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
		       mypktptr->acknum,  mypktptr->checksum);
		for (i = 0; i < 20; i++)
			printf("%c", mypktptr->payload[i]);
		printf("\n");
	}

	/* create future event for arrival of packet at the other side */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
	evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
	evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
	
	lastime = time;
	
	for (q = evlist; q != NULL ; q = q->next)
		if ( (q->evtype == FROM_LAYER3  && q->eventity == evptr->eventity) )
			lastime = q->evtime;
	evptr->evtime =  lastime + 1 + 9 * jimsrand();



	
	if (jimsrand() < corruptprob)  {
		ncorrupt++;
		if ( (x = jimsrand()) < .75)
			mypktptr->payload[0] = 'Z'; 
		else if (x < .875)
			mypktptr->seqnum = 999999;
		else
			mypktptr->acknum = 999999;
		if (TRACE > 0)
			printf("          TOLAYER3: packet being corrupted\n");
	}

	if (TRACE > 2)
		printf("          TOLAYER3: scheduling arrival on other side\n");
	insertevent(evptr);
}

tolayer5(AorB, datasent)
int AorB;
char datasent[20];
{
	int i;
	if (TRACE > 2) {
		printf("          TOLAYER5: data received: ");
		for (i = 0; i < 20; i++)
			printf("%c", datasent[i]);
		printf("\n");
	}

}