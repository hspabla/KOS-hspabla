
#include "ringbuf.h" 

Queue::Queue(int size) {
    head = tail = -1;
    Queue::size = QUEUE_SIZE;
    count = 0;
}

Queue::~Queue(){
    //delete [] data; 	
}

int Queue::size() {
		return count;
	}
 
void Queue::push(sampleD item) {
    if (head == 0 && tail == size || head == tail + 1) {
        cout << "Queue is full\n";
    }
    else if (head == -1 && tail == -1) {
        head = 0;
        tail = 0;
        ringArray[head] = item;
        count++;
    }
    else if (tail == size) {
        tail = 0;
        ringArray[tail] = item;
        count++;
    }
    else {
        tail++;
        ringArray[tail] = item;
        count++;
    }
}
 
void Queue::pop() {
    if (head == -1 && tail == -1) {
        cout << "Queue is empty\n";
    }
    else {
        if (head == tail) {
        //ringArray[head] = NULL;;
        head = -1;
        tail = -1;
        count--;
    }
    else if (head == size) {
        //ringArray[head] = NULL;
        head = 0;
        count--;
    }
    else {
        //ringArray[head] = NULL;
        head++;
        count--;
    }
    }
}
 
void Queue::print() {
    if (count == 0) {
        cout << "Queue is empty\n";
    } else {
        for(int i = 0; i < size + 1; i++)
	    printf("%lu\n",ringArray[i].address);
            //cout << ringArray[i]->address << " ";
        //cout << endl;
    }
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

