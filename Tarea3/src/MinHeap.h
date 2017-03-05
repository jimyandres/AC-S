#ifndef MIN_HEAP_H
#define MIN_HEAP_H

#include <iostream>
#include <vector>
#include <string>

using namespace std;

struct Server {
	string address;
	long bytes_transmitting;
	long space_used;
	long key;
};

template <typename T>
class MinHeap {
	int size;
	vector<T> A;

public:
	MinHeap() {
		size=0;
	}

	MinHeap(int n, const vector<T> B) {
		size=n;

		for(int i=0; i<n; i++) {
			A.push_back(B[i]);
		}

		for(int i=size/2; i>=0; i--) {
			MinHeapify(i);
		}
	}

	~MinHeap() {
	}

	int getSize() {
		return size;
	}

	vector<T> getHeap() {
		return A;
	}

	int left(int i) {
		return i*2+1;
	}

	int right(int i) {
		return i*2+2;
	}

	int parent(int i) {
		return (i-1)/2;
	}

	void MinHeapify(int i) {
		int l=left(i);
		int r=right(i);
		int smallest;
		if(l<=size && A[l-1].key<A[i].key) {
			smallest=l-1;
		} 
		else {
			smallest=i;
		}
		if(r<=size && A[r-1].key<A[smallest].key) { 
			smallest=r-1;
		}
		if(smallest!=i) {
			swap(A[i],A[smallest]);
			MinHeapify(smallest);
		}
	}

	void MinHeapifyPush(int i) {
		while(i>0 && A[parent(i)].key > A[i].key) {
			swap(A[i], A[parent(i)]);
			i=parent(i);
		}
	}

	void insert(const T& input) {
		size++;
		A.resize(size);
		A[size-1] = input;
		MinHeapifyPush(size-1);
	}

	T top() {
		return A[0];
	}

	T pop() {
		T min;
		if(size>0) {
			min=A[0];
			A[0]=A[size-1];
			A.resize(size-1);
			size = A.size();
			MinHeapify(0);
			return min;
		}
	}

	int search(T target) {
		for(int i=0; i<size; i++) {
			if(target.key == A[i].key) {
				return i;
			}
		}
		return -1;
	}

	int search(string address) {
		for(int i=0; i<size; i++) {
			if(address == A[i].address) {
				return i;
			}
		}
		return -1;
	}

	T deleteAt(int i) {
		T deleted = A[i];
		A[i] = A[size-1];
		A.resize(size-1);
		size = A.size();

		if(size > 0 && i!=size) {
			int tmp_index = i;
			while(tmp_index>0 && A[parent(tmp_index)].key > A[tmp_index].key) {
				swap(A[tmp_index], A[parent(tmp_index)]);
				tmp_index=parent(tmp_index);
			}

			if (tmp_index == i) {
				MinHeapify(tmp_index);
			}
		}
		return deleted;
	}
};

#endif

/*int main()
{
	int n; //numbers of servers to add
	cin>>n;
	Server input, tmp;
	***********************
	initializing dinamically
	***********************
	MinHeap<Server> servers;
	for(int i=0; i<n; i++) {
		cin >> input.address;
		cin >> input.bytes_transmitting;
		cin >> input.space_used;
		input.key = ((double)input.bytes_transmitting*0.5)+((double)input.space_used*0.5);
		servers.insert(input);
	}

	*****************************************
	initializing from existing list of servers
	*****************************************
	*string address;
	long bytes_transmitting, space_used;
	vector<Server> A(n);
	for(int i=0; i<n; i++) {
		cin >> address;
		cin >> bytes_transmitting;
		cin >> space_used;
		input.key = ((double)bytes_transmitting*0.5)+((double)space_used*0.5);
		A[i] = input;
	}
	MinHeap<Server> servers(n, A);*

	cout << "initial state: \n";
	HeapSort(servers);

	****************************************************************************
	request the first server, change its properties, and put it back in the queue
	****************************************************************************
	*tmp = servers.pop();
	cout << "result from pop: \n";
	HeapSort(servers);
	//change properties
	tmp.space_used +=  tmp.bytes_transmitting;
	tmp.bytes_transmitting = 5242880;
	tmp.key = ((double)tmp.bytes_transmitting*0.5)+((double)tmp.space_used*0.5);
	//insert the server with new properties
	servers.insert(tmp);
	cout << "result after reinsert: \n";
	HeapSort(servers);*

	*********************************************************************************
	find and specific server (one that has disconnected), and remove it from the queue
	*********************************************************************************

	*string query_add = "tcp://127.0.0.1:5557";
	int query = servers.search(query_add);
	if(query < 0) {
		cout << "Server not registered on queue" << endl;
	} else {
		cout <<"server running in " << query_add << " is at index: " << query << endl;
		servers.deleteAt(query);
		cout << "Server " << query_add << " removed!" << endl;
		HeapSort(servers);
	}*

	return 0;
}*/
