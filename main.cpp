#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lcgrand.h"

#define ARRIVAL 0
#define DEPARTURE 1
#define SERVER_DEPARTURE 2

#define T 0
#define B 1
#define F 2

#define ENCDER_IDLE 0
#define ENCDER_BUSY 1

#define SERVER_IDLE 0  
#define SERVER_BUSY 1  

struct event{
	float time;
	int eventType;
	int fieldType;
	float complexity;

	event* nextEventPtr;
	event* exEventPtr;
};
typedef event eventNode;



/*---------------operate event list-----------------*/
eventNode* newEventNode(int, int, float);                   // create a new event
void eventInsert(eventNode *node);                          // insert a new event to the event list
void timing();                                              // time function used to search the min time in the event list
void eventDelete(eventNode *deletePosition);                // find the min_time_node, delete it from the event list
void eventArrival();                                        // Encoder : event arrival
void eventDeparture();                                      // Encoder : event departure
/*---------------operate event list-----------------*/

/*---------------operate encoder queue -------------*/
eventNode *queue_head = NULL;                               // encoder queue head
eventNode *queue_tail = NULL;                               // encoder queue tail
void queueInsert(eventNode *wait_in_queueNode);             // insert field to queue to wait encoder encodes
void queueDelete();                                         // encoder can encode the field
int lastDrop = 0;                                           // last drop field  1 : T   0:T,B
/*---------------operate encoder queue -------------*/

/*-------------------server queue ------------------*/
eventNode *serverQueue_head = NULL;
eventNode *serverQueue_tail = NULL;
void serverQueueInsert(eventNode *depar_from_encoderNode);  // insert field to queue to wait encoder encodes
void serverQqueueDelete();

/*-------------------server queue ------------------*/

void initialize();                                          // initial simulation environment
float expon(float mean);                                    // Exponential variate generation function.

 /* ----------simualtion static setting-------------*/
int queueSize = 21;                                         // encoder buffer size 41 61 81 101
float mean_arrival = 1 / 59.94;                             // inter arrival              sec      (1/50)
float complexity_field = 262.5;                             // complexity of a field      fob      (312.5)
float end_time = 28800;                                     // end simulation time
int C_encoder = 15800;                                      // encoder capacity           fob/s
int C_storage = 1600;                                       // server capacity            byte/s
float combine_alpha = 0.1;                                  // combine T and B field to a viedo frame parameters 
/* ----------simualtion static setting-------------*/

/*-------------------varialbe----------------------*/
eventNode *head = NULL;                                     // eventlist first node
eventNode *tail = NULL;                                     // eventlist last node
int total_N_Field = 0;                                      // total number of the field in the simulation time
int drop_N_Field = 0;                                       // total dropped number of the field int the simulation time
int enocder_status = 0;                                     // encoder status
int server_status = 0;                                      // server status
float sim_time = 0;                                         // simulation time
int counter_queue = 0;                                      // number in encoder buffer 
float last_event_time = 0;                                  // record the last event time
int frame = 0;                                              // ouput frame number
float printTime = 1800;                                     // output to file second
float serverWorkTime = 0;
/*-------------------varialbe----------------------*/
void report();
void releaseRe(eventNode *Q_head, eventNode *Q_tail);       // release resource to system
FILE *outfile;
//FILE *outfileU;
int main(int argc, char** argv) {

	// static setting need to change to simulation , so it has to record the initial value for recovery
	float tempArrival = mean_arrival;
	float tempComplex = complexity_field;
	int tempQSize = queueSize;
	int tempOutputT = printTime;
	
	outfile = fopen("output3.xls","w");
	//outfileU = fopen("outputu.xls", "w");
	for (int marrival = 0; marrival < 2; marrival++) {
		mean_arrival = tempArrival + marrival * 0.003316;

		for (int hcomplexity = 0; hcomplexity < 2; hcomplexity++) {
			complexity_field = tempComplex + hcomplexity *50;
			fprintf(outfile,"\t0.5h\t1.0h\t1.5h\t2.0h\t2.5h\t3.0h\t3.5h\t4.0h\t4.5h\t5.0h\t5.5h\t6.0h\t6.5h\t7.0h\t7.5h\t8.0h\n");
			for (int Qsize = 0; Qsize < 5; Qsize++) {
				queueSize = tempQSize + Qsize * 20;
				initialize();
				fprintf(outfile, "£] : %d\t", queueSize - 1);
				//fprintf(outfileU, "\t");
				while (sim_time <= end_time) {
					timing();
					if (sim_time >= printTime) {
						fprintf(outfile, "%f\t", (drop_N_Field*0.1 / total_N_Field) * 100);
						//fprintf(outfileU, "%f\t", (serverWorkTime / sim_time) * 100);
						printTime += 1800;
					}
				}
				fprintf(outfile, "\n");		
				report();
				printf("22drop field number : \t%f \n", (drop_N_Field*1.0 / total_N_Field*1.0)*100);
				releaseRe(queue_head , queue_tail);
				releaseRe(serverQueue_head , serverQueue_tail);
				releaseRe(head , tail);			
			}
		}
	}
	fclose(outfile);
	system("pause");
	return 0;
}

/**
* initial simulation
**/
void initialize()
{

	// initial simulation time clock
	sim_time = 0.0;

	// initial simulation status
	enocder_status = ENCDER_IDLE;
	server_status = SERVER_IDLE;
	counter_queue = 0;
	last_event_time = 0.0;
	total_N_Field = 0;
	drop_N_Field = 0;
	frame = 0;
	lastDrop = 0;
	printTime = 1800;
	serverWorkTime = 0;

	queue_head = NULL;                               
	queue_tail = NULL;
	serverQueue_head = NULL;
	serverQueue_tail = NULL;
	head = NULL;
	tail = NULL;

	// Initialize event list, set the first event occur
	float field_complexity = expon(complexity_field);                    // encoder encoding time
	eventNode *firstEvent = newEventNode(ARRIVAL, T , field_complexity);
	eventInsert(firstEvent);
}

/*
* time master 
* move the simulation time to the event time which hsa the least sim time
* three event action: ARRIVAL, DEPARTURE , SERVER_DEPARTURE 
**/
void timing() {
	float min_time_next_event = 1.0e+29;
	eventNode *mintime_position = NULL;
	eventNode *p;

	// fine the event which has min scheduled time 
	for (p = head; p != NULL; p = p->nextEventPtr) {
		if (p->time < min_time_next_event) {
			mintime_position = p;
			min_time_next_event = p->time;
		}
	}

	
	if (mintime_position->eventType == ARRIVAL) {                                      // now is arrival
		last_event_time = sim_time;
		sim_time = mintime_position->time;

		//schedule next arrival
		if (mintime_position->fieldType == T) {
			float field_complexity = expon(complexity_field);                         
			eventNode *nextEvent = newEventNode(ARRIVAL, B, field_complexity);         // now is Top  , next is Buttom
			eventInsert(nextEvent);
		}
		else if (mintime_position->fieldType == B) {
			float field_complexity = expon(complexity_field);                          // now is Buttom  , next is Top
			eventNode *nextEvent = newEventNode(ARRIVAL, T, field_complexity);
			eventInsert(nextEvent);
		}

		// encoding
		if (enocder_status == ENCDER_IDLE) {
			mintime_position->time = sim_time + mintime_position->complexity / C_encoder;
			mintime_position->eventType = DEPARTURE;
			enocder_status = ENCDER_BUSY;
		}
		else {
			eventDelete(mintime_position);                                             // because of the encoder is busy , so delete arrival event from event list
			queueInsert(mintime_position);                                             // to waiting queue
																					   
		}
		total_N_Field++;
	}
	else if (mintime_position->eventType == DEPARTURE) {                               //event departure
														                               
		last_event_time = sim_time;
		sim_time = mintime_position->time;

		eventDelete(mintime_position);                                                 // deleteEvent from event list
		serverQueueInsert(mintime_position);                                           // eventNode into server queue

		if (mintime_position->fieldType == B  && server_status == SERVER_IDLE) {
			serverQqueueDelete();
			server_status = SERVER_BUSY;
		}

		if (queue_head != NULL) {                                                      // if queue not idle , there is a waiting eventNode
			queueDelete();
		}
		else {
			enocder_status = ENCDER_IDLE;
		}
	}
	else if (mintime_position->eventType == SERVER_DEPARTURE) {
		last_event_time = sim_time;
		sim_time = mintime_position->time;
		if (serverQueue_head != serverQueue_tail) {
			serverQqueueDelete();
		}
		else {
			server_status = SERVER_IDLE;
		}
		eventDelete(mintime_position);
		frame++;
	}

}

/*
* create new eventNode
* every arrival event happen , create a new event arrival 
* or
* server arrival create a new serverNode for server departure
* @ eventType , fieldType , field_complexity
*/
eventNode * newEventNode(int eventType, int fieldType , float field_complexity) {
	float arrival_time = expon(mean_arrival);

	eventNode *node = (eventNode*)malloc(sizeof(eventNode));
	node->time = sim_time + arrival_time;
	node->complexity = field_complexity;
	node->eventType = eventType;
	node->fieldType = fieldType;
	node->nextEventPtr = NULL;
	node->exEventPtr = NULL;

	return node;
}

/**
* add eventNode to  eventlist
* add a eventNode to event list to wait occuring 
* @ node  : insert node
**/
void eventInsert(eventNode *node) {
	if (head == NULL) {
		head = node;
		tail = node;
	}
	else {
		tail->nextEventPtr = node;
		node->exEventPtr = tail;
		tail = node;
	}
}

/**
* delete eventNode from event list
* delete the min simulation time event node from the event list
* @ deletePosition : position of the min simulation time node 
**/
void eventDelete(eventNode *deletePosition) {
	eventNode *temp;
	temp = deletePosition;

	if (deletePosition->exEventPtr == NULL && deletePosition->nextEventPtr == NULL) { // only one event in event list
		head = NULL;
		tail = NULL;
	}
	else if (deletePosition->exEventPtr == NULL) {                                    // deletePosition is head
		head = deletePosition->nextEventPtr;
		head->exEventPtr = NULL;
	}
	else if (deletePosition->nextEventPtr == NULL) {                                  // deletePosition is tail
		tail = deletePosition->exEventPtr;
		tail->nextEventPtr = NULL;
	}
	else {                                                                           // deletePosition is in middle
		deletePosition->exEventPtr->nextEventPtr = deletePosition->nextEventPtr;
		deletePosition->nextEventPtr->exEventPtr = deletePosition->exEventPtr;
	}
	if (temp->eventType == SERVER_DEPARTURE) {
		free(temp);
	}
}

/**
* waiting to encode
* encoder is busy so arrival event node is not able to encode , 
* so let it wait
* @ wait_in_queueNode : arrival event node which needs to wait in queue
**/
void queueInsert(eventNode *wait_in_queueNode){
	if (lastDrop == 1) {
		free(wait_in_queueNode);
		counter_queue--;
		drop_N_Field++;
		lastDrop = 0;
	}
	else {
		counter_queue++;
		if (counter_queue <= queueSize) {
			wait_in_queueNode->exEventPtr = NULL;
			wait_in_queueNode->nextEventPtr = NULL;
			if (queue_head == NULL) {
				queue_head = wait_in_queueNode;
				queue_tail = wait_in_queueNode;
			}
			else {
				queue_tail->nextEventPtr = wait_in_queueNode;
				wait_in_queueNode->exEventPtr = queue_tail;
				queue_tail = wait_in_queueNode;
			}
		}
		else { // queue if full
			if (wait_in_queueNode->fieldType == T) { // if waiting event is top , drop top
				free(wait_in_queueNode);
				counter_queue--;
				drop_N_Field++;
				lastDrop = 1;
			}
			else { // if waiting event is buttom , drop top and buttom
				lastDrop = 0;
				eventNode *temp;
				temp = queue_tail;
				queue_tail = queue_tail->exEventPtr;
				queue_tail->nextEventPtr = NULL;
				free(temp);
				free(wait_in_queueNode);
				counter_queue -= 2;
				drop_N_Field += 2;
			}
		}
	}
}

/**
* time to wake up
* encoder is free to encoder the event node , pop the first node in queue to encode
**/
void queueDelete() {
	eventNode *temp;
	temp = queue_head;
	
	if (queue_head == queue_tail) {
		queue_head = NULL;
		queue_tail = NULL;
	}
	else {
		queue_head = queue_head->nextEventPtr;
	}

	temp->nextEventPtr = NULL;
	temp->exEventPtr = NULL;

	temp->time = sim_time + temp->complexity / C_encoder;                            // encoder encoding time
	temp->eventType = DEPARTURE;

	eventInsert(temp);
	counter_queue--;
}

/**
* waiting to server
* add the event node to server queue , waiting for server 
* @ depar_from_encoderNode : the field which is encoded by encoder
**/
void serverQueueInsert(eventNode *depar_from_encoderNode) {
	
	depar_from_encoderNode->exEventPtr = NULL;
	depar_from_encoderNode->nextEventPtr = NULL;

	if (serverQueue_head == NULL) {
		serverQueue_head = depar_from_encoderNode;
		serverQueue_tail = depar_from_encoderNode;
	}
	else {
		serverQueue_tail->nextEventPtr = depar_from_encoderNode;
		depar_from_encoderNode->exEventPtr = serverQueue_tail;
		serverQueue_tail = depar_from_encoderNode;
	}
	if (serverQueue_head->fieldType == B) {
		//getchar();
	}
}

/**
* server process two fields
* when server is available to process , pop the first two fields to server
**/
void serverQqueueDelete() {
	eventNode *node_T , *node_B;
	node_T = serverQueue_head;
	node_B = serverQueue_head->nextEventPtr;
	
	
	if (serverQueue_head->nextEventPtr == serverQueue_tail) {                 // only a pair (T,B) in the server queue
		serverQueue_head = NULL;
		serverQueue_tail = NULL;
	}
	else {
		serverQueue_head = node_B->nextEventPtr;
		serverQueue_head->exEventPtr = NULL;
	}

	float frameSize = (node_T->complexity + node_B->complexity)*combine_alpha;
	eventNode *frameNode = newEventNode(SERVER_DEPARTURE, F, frameSize);
	frameNode->time = sim_time + frameSize / C_storage;
	serverWorkTime += frameSize / C_storage;
	eventInsert(frameNode);
	free(node_T);
	free(node_B);
}

/**
* exponitial random number
* @ mean
**/
float expon(float mean) {
	return -mean * log(lcgrand(1));
}

/**
* delete every node
* @ Q_head : list head  Q_tail : list tail
**/
void releaseRe(eventNode *Q_head, eventNode *Q_tail) {
	eventNode *temp = NULL;
	while (1) {
		temp = Q_head;
		if (Q_head != Q_tail) {		
			Q_head = Q_head->nextEventPtr;
			free(temp);
		}
		else {
			free(temp);
			break;
		}
	}
	
}

void report() {
	printf("\n/** end simulation **/\n");
	printf("mean_arrival : %f\tcomplexity_field : %f\tbufferSize : %d\n" , mean_arrival , complexity_field , queueSize);
	printf("total field number : \t%d \n", total_N_Field);
	printf("drop field number : \t%f \n", (drop_N_Field*1.0 / total_N_Field*1.0) * 100);
	printf("number of frame : \t%d \n", frame);
	printf("server utilization: \t%f \n", (serverWorkTime/sim_time)*100);
}