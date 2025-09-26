/* 
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Wyatt Soper
	UIN: 733006142
	Date: 9/16/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <fstream>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m = MAX_MESSAGE;
	bool new_chan = false;
	vector<FIFORequestChannel*> channels;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi(optarg);
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}

	// fork
	pid_t pid = fork();
	if (pid == 0) {
    	// give arguments for the server
		char* args[5];
    	args[0] = (char*)"./server";
    	args[1] = (char*)"-m";
    	string m_str = to_string(m);
    	args[2] = (char*)m_str.c_str();
    	args[3] = NULL;

		// In the child, run execvp using the server arguments.
    	execvp(args[0], args);
    	perror("execvp failed");
    	exit(1);
	}
	else if (pid < 0) {
    	perror("fork failed");
    	exit(1);
	}

    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&cont_chan);

	if (new_chan) {
		// send newchannel request to the server;
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
    	cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));

		// create a variable to hold the name
		char new_name[100];
		cont_chan.cread(new_name, sizeof(new_name));

		// call the FIFORequestChannel constructor with the name from the server
		FIFORequestChannel* new_channel = new FIFORequestChannel(new_name, FIFORequestChannel::CLIENT_SIDE);

		// Push the new channel into the vector
		channels.push_back(new_channel);
	}

	FIFORequestChannel& chan = *(channels.back());
	
	// Single datapoint, only run p.t.e != -1
	if (p != -1 && t != -1 && e != -1) {
    	char buf[MAX_MESSAGE];
    	datamsg x(p, t, e);
	
		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); 
		double reply;
		chan.cread(&reply, sizeof(double)); 
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}

	// Else, if p != -1, request 1000 datapoints.
	else if (p != -1){
		ofstream ofs("received/x1.csv");
		if (!ofs.is_open()) {
			cerr << "x1 cannot be oppened" << endl;
			exit(1);
		}
		for (int i = 0; i < 1000; i++) {
			double time = i * 0.004;

			datamsg req1(p, time, 1);
            chan.cwrite(&req1, sizeof(datamsg));
            double ecg1;
            chan.cread(&ecg1, sizeof(double));

			datamsg req2(p, time, 2);
            chan.cwrite(&req2, sizeof(datamsg));
            double ecg2;
            chan.cread(&ecg2, sizeof(double));

			ofs << time << "," << ecg1 << "," << ecg2 << "\n";
		}
        ofs.close();
    }
	

    if (!filename.empty()){	
		filemsg fm(0, 0);
	
		int len = sizeof(filemsg) + (filename.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), filename.c_str());
		chan.cwrite(buf2, len);	// I want the file length;

		int64_t filesize = 0;
		chan.cread(&filesize, sizeof(int64_t));

		// open output file in directory
		string op = "received/" + filename;
    	ofstream outfile(op, ios::binary);
    	if (!outfile.is_open()) {
     	   cerr << "Could not open output file: " << op << endl;
        	exit(1);
    	}

		char* buf3 = new char[m];	// create buffer of size buff capacity (m)

		int64_t offset = 0;

		// Loop over the segments in the file filesize / buff capacity
		while (offset < filesize) {
    		int64_t chunk = min((int64_t)m, filesize - offset);

			// create filemsg instance
        	filemsg* file_req = (filemsg*)buf2;
        	file_req->offset = offset;	// set offset in the file
        	file_req->length = chunk;	// set the length. Be careful of the last segment

			// send the request (buf2)
			chan.cwrite(buf2, len);

			// receive the response
        	// cread into buf3 length file_req->len
        	chan.cread(buf3, chunk);

			// write buf3 into file: received/filename
			outfile.write(buf3, chunk);
			
        	offset += chunk;
		}

		outfile.close();

		delete[] buf2;
		delete[] buf3;
	}

	// If necessary, close and delete the new channel
	if(new_chan)
	{
		MESSAGE_TYPE q = QUIT_MSG;
		channels.back()->cwrite(&q, sizeof(MESSAGE_TYPE));
		delete channels.back();
		channels.pop_back();
	}
	
	// closing the control channel    
    MESSAGE_TYPE mes = QUIT_MSG;
    cont_chan.cwrite(&mes, sizeof(MESSAGE_TYPE));
}