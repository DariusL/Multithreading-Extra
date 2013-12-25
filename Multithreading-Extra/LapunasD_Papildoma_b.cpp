#include <mpi.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <Windows.h>

using namespace std;

typedef size_t uint;

MPI_Status status;

const string file = "LapunasD.txt";
enum CODE
{
	CODE_CONSUMER = 1 << 0,
	CODE_PRODUCER = 1 << 1,
	CODE_DONE = 1 << 2
};

struct Data
{
	Data(string input);
	char pav[10];
	int kiekis;
	double kaina;
	string Print(uint nr);
};

Data::Data(string input)
{
	uint start, end;
	start = 0;
	end = input.find(' ');
	memcpy(pav, input.substr(0, end).c_str(), 10);
	start = end + 1;
	end = input.find(' ', start);
	kiekis = stoi(input.substr(start, end - start));
	start = end + 1;
	kaina = stod(input.substr(start));
}

string Data::Print(uint nr)
{
	stringstream ss;
	ss << setw(3) << nr << setw(15) << pav << setw(7) << kiekis << setw(20) << kaina;
	return ss.str();
}

struct Counter
{
	Counter() :count(0){}
	Counter(string line);
	Counter(Data &data);
	char pav[10];
	int count;
public:
	int operator++(){ return ++count; }
	int operator--(){ return --count; }
	bool operator==(const Counter &other){ return strcmp(pav, other.pav) == 0; }
	bool operator <(const Counter &other){ return strcmp(pav, other.pav)  > 0; }
	bool operator >(const Counter &other){ return strcmp(pav, other.pav)  < 0; }
	string Print(uint nr);
};

Counter::Counter(string line)
{
	uint start, end;
	start = 0;
	end = line.find(' ');
	memcpy(pav, line.substr(0, end).c_str(), 10);
	start = end + 1;
	end = line.find(' ', start);
	count = stoi(line.substr(start, end - start));
}

Counter::Counter(Data &data)
{
	memcpy(pav, data.pav, 10);
	count = data.kiekis;
}

string Counter::Print(uint nr)
{
	stringstream ss;
	ss << setw(15) << pav << setw(5) << count;
	return ss.str();
}

struct Job
{
	bool consume;
	int nr;
};

struct Message
{
	int code;
	int sender;
	Counter data;
};

class Buffer
{
	vector<Counter> buffer;
	vector<bool> active;
	const int producerCount, consumerCount;
	int producerStart, consumerStart;
public:
	Buffer(uint producerCount, uint consumerCount);
	void Start();
	string Print();
private:
	void Add(Counter c);
	int Take(Counter c);
};

Buffer::Buffer(uint producerCount, uint consumerCount)
:producerCount((int)producerCount), consumerCount((int)consumerCount),
producerStart(1), consumerStart((int)producerCount + 1),
active(consumerCount + producerCount + 1, true)
{
}

void Buffer::Start()
{
	Message msg;
	int consumer = consumerStart;
	int producer = producerStart;
	int activeProducers = producerCount;
	int activeConsumers = consumerCount;
	while (activeProducers + activeConsumers)
	{
		if (activeConsumers && (buffer.size() || activeProducers == 0))
		{
			if (active[consumer])
			{
				MPI_Recv(&msg, sizeof(msg), MPI_BYTE, consumer, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				if (msg.code & CODE_DONE)
				{
					active[consumer] = false;
					activeConsumers--;
				}
				else
				{
					int taken = Take(msg.data);
					if (activeProducers == 0)
						msg.code |= CODE_DONE;
					msg.data.count = taken;
					MPI_Send(&msg, sizeof(msg), MPI_BYTE, consumer, 0, MPI_COMM_WORLD);
				}
			}
			consumer++;
			if (consumer >= consumerCount + consumerStart)
				consumer = consumerStart;
		}
		if (activeProducers)
		{
			if (active[producer])
			{
				MPI_Recv(&msg, sizeof(msg), MPI_BYTE, producer, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				if (msg.code & CODE_DONE)
				{
					active[producer] = false;
					activeProducers--;
				}
				else
				{
					Add(msg.data);
				}
			}
			producer++;
			if (producer >= producerCount + producerStart)
				producer = producerStart;
		}
	}
}

void Buffer::Add(Counter c)
{
	//randamas atitinkamo pavadinimo skaitliukas
	auto i = find(buffer.begin(), buffer.end(), c);
	if (i != buffer.end())
	{
		(*i).count += c.count;
	}
	else
	{
		//jei skaitliuko neradome, kuriame nauja reikiamoje vietoje
		auto size = buffer.size();
		for (auto i = buffer.begin(); i != buffer.end(); i++)
		{
			if (c < (*i))
			{
				buffer.insert(i, c);
				break;
			}
		}
		if (buffer.size() == size)
			buffer.push_back(c);
	}
}

int Buffer::Take(Counter c)
{
	//randamas atitinkamas skaitliukas
	auto i = find(buffer.begin(), buffer.end(), c);
	int taken = 0;
	if (i != buffer.end())
	{
		//paimame kiek imanoma
		if ((*i).count >= c.count)
			taken = c.count;
		else
			taken = (*i).count;
		(*i).count -= taken;

		//triname tuscia skaitliuka
		if ((*i).count <= 0)
			buffer.erase(i);
	}
	return taken;
}

string Buffer::Print()
{
	stringstream ss;
	for (auto &c : buffer)
		ss << c.pav << " " << c.count << endl;
	return ss.str();
}

vector<vector<Data>> ReadStuff(string file);
vector<vector<Counter>> ReadCounters(string file);
vector<string> ReadLines(string file);
string Titles();
string Print(int nr, Data &s);
string Print(Data &data);
void Make(vector<Data> stuff, int rank);
void Use(vector<Counter> stuff, int rank);
void SendJobs(uint &consumers, uint &producers);

template <typename T>
void ForEachForEach(vector < vector < T > > &data, string perVec);

int main(int argc, char *argv[])
{
	MPI_Init(&argc, &argv);

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

#ifdef _DEBUG
	if (rank == 0)
		MessageBox(nullptr, L"", L"Attach debugger", MB_OK);
#endif
	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0)
	{
		uint consumers, producers;
		SendJobs(consumers, producers);
		Buffer buffer(producers, consumers);
		buffer.Start();
		cout << endl << buffer.Print();
	}
	else
	{
		Job job;
		MPI_Recv(&job, sizeof(Job), MPI_BYTE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		if (job.consume)
		{
			Use(ReadCounters(file)[job.nr], rank);
		}
		else
		{
			Make(ReadStuff(file)[job.nr], rank);
		}
	}
	MPI_Finalize();
	return 0;
}


void SendJobs(uint &consumerCount, uint &producerCount)
{
	auto producers = ReadStuff(file);
	auto consumers = ReadCounters(file);

	cout << "\nGamintojai\n\n";
	ForEachForEach(producers, "Gamintojas_");
	cout << "\nVartotojai\n\n";
	ForEachForEach(consumers, "Vartotojas_");

	int r = 1;
	Job job;
	job.consume = false;
	for (int i = 0; i < producers.size(); i++)
	{
		job.nr = i;
		MPI_Send(&job, sizeof(Job), MPI_BYTE, r, 0, MPI_COMM_WORLD);
		r++;
	}
	job.consume = true;
	for (int i = 0; i < consumers.size(); i++)
	{
		job.nr = i;
		MPI_Send(&job, sizeof(Job), MPI_BYTE, r, 0, MPI_COMM_WORLD);
		r++;
	}
	consumerCount = consumers.size();
	producerCount = producers.size();
}

//gamintoju skaitymas, modifikuotas veikti su naujais vartotoju duomenimis
vector<vector<Data>> ReadStuff(string file)
{
	auto lines = ReadLines(file);
	vector<vector<Data>> ret;
	vector<Data> tmp;
	for (unsigned int i = 0; i < lines.size(); i++)
	{
		if (lines[i] == "vartotojai")
		{
			break;
		}
		if (lines[i] == "")
		{
			ret.push_back(move(tmp));
		}
		else
		{
			tmp.emplace_back(lines[i]);
		}
	}
	return ret;
}

vector<string> ReadLines(string file)
{
	vector<string> ret;
	ifstream duom(file);
	while (!duom.eof())
	{
		string line;
		getline(duom, line);
		ret.push_back(line);
	}
	return ret;
}

//vartotoju duomenu skaitymas
vector<vector<Counter>> ReadCounters(string file)
{
	auto lines = ReadLines(file);
	vector<vector<Counter>> ret;
	vector<Counter> tmp;
	unsigned int i;
	for (i = 0; i < lines.size(); i++)
	{
		if (lines[i] == "vartotojai")
			break;
	}
	for (i++; i < lines.size(); i++)
	{
		if (lines[i] == "")
			ret.push_back(move(tmp));
		else
			tmp.emplace_back(lines[i]);
	}
	return ret;
}

template <typename T>
void ForEachForEach(vector < vector < T > > &data, string perVec)
{
	for (uint i = 0; i < data.size(); i++)
	{
		auto &vec = data[i];
		cout << perVec << i << endl;
		for (uint j = 0; j < vec.size(); j++)
		{
			cout << vec[j].Print(j) << endl;
		}
	}
	cout << endl;
}

string Print(int nr, Data &s)
{
	stringstream ss;
	ss << setw(3) << nr << Print(s);
	return ss.str();
}

string Titles()
{
	stringstream ss;
	ss << setw(15) << "Pavadiniams" << setw(7) << "Kiekis" << setw(20) << "Kaina";
	return ss.str();
}

string Print(Data &data)
{
	stringstream ss;
	ss << setw(15) << data.pav << setw(7) << data.kiekis << setw(20) << data.kaina;
	return ss.str();
}

//gamybos funkcija
void Make(vector<Data> stuff, int rank)
{
	Message msg;
	msg.code = CODE_PRODUCER;
	msg.sender = rank;
	for (auto &s : stuff)
	{
		msg.data = Counter(s);
		MPI_Send(&msg, sizeof(msg), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
	}
	msg.code |= CODE_DONE;
	MPI_Send(&msg, sizeof(msg), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
}

//vartojimo funkcija
void Use(vector<Counter> stuff, int rank)
{
	auto i = stuff.begin();
	vector<Counter> ret;
	Message msg;
	msg.code = CODE_CONSUMER;
	msg.sender = rank;
	while (stuff.size() > 0)
	{
		msg.data = *i;
		msg.code = CODE_CONSUMER;
		MPI_Sendrecv(&msg, sizeof(msg), MPI_BYTE, 0, 0, &msg, sizeof(msg), MPI_BYTE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		(*i).count -= msg.data.count;
		if ((*i).count <= 0)
		{
			stuff.erase(i);
			i = stuff.begin();
		}
		else if (msg.code & CODE_DONE)
		{
			ret.push_back(*i);
			stuff.erase(i);
			i = stuff.begin();
		}
		else
		{
			i++;
			if (i >= stuff.end())
				i = stuff.begin();
		}
	}
	for (auto c : ret)
		cout << setw(15) << c.pav << setw(5) << c.count << endl;
	msg.code |= CODE_DONE;
	MPI_Send(&msg, sizeof(msg), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
}