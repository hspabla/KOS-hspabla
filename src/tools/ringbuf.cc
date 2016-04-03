
#include "ringbuf.h" 

Queue::Queue() {
    head = tail = -1;
    Queue::size = QUEUE_SIZE;
    count = 0;
}

Queue::~Queue(){
    //delete [] data; 	
}

int Queue::qsize() {
		return count;
	}


void Queue::push(sampleD item) {
   if (head == -1 && tail == -1) {
       head = 0;
       tail = 0;
       ringArray[head] = item;
       count++;
   }
   else if (tail == (size-1)){
       tail = (tail+1) % size;
       count=1;
       ringArray[tail] = item;
   }
   else {
       tail = (tail+1) % size;
       ringArray[tail] = item;
       count++;
   }
}

uint64_t Queue::view(int i) {
    return ringArray[i].address;
}

void Queue::data_read() {
    Queue::tail = 0;
    Queue::count = 0;
} 



/*
int main() {
    int keyCode = 0, size = 0;
    int tmpInput;
 
 //   cout << "Enter size of queue: ";
 //   cin >> size;
 	
//To create new instance
    size = QUEUE_SIZE;
    Queue queue(size);
//
    while (true) {
        cout << "Enter 1 for add, 2 for deletion, 3 for display, 4 for exit.\n";
        cin >> keyCode;
 
        switch (keyCode) {
        case 1:
            cout << "Enter number: ";
            cin >> tmpInput;
            queue.push(tmpInput);
            break;
        case 2:
            queue.pop();
            break;
        case 3:
            queue.print();
            break;
        case 4:
            exit(0);
            break;
        default:
            break;
        }
    }
}

*/

