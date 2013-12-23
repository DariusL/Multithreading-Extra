#include <mpi.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;
typedef size_t uint;
enum CODE
{
	CODE_CONSUMER = 1 << 0,
	CODE_PRODUCER = 1 << 1,
	CODE_DONE     = 1 << 2
};

struct Data
{
	char pav[10];
	int kiekis;
	double kaina;
};

Data MakeData(string input)
{
	Data data;
	uint start, end;
	start = 0;
	end = input.find(' ');
	memcpy(data.pav, input.substr(0, end).c_str(), 10);
	start = end + 1;
	end = input.find(' ', start);
	data.kiekis = stoi(input.substr(start, end - start));
	start = end + 1;
	data.kaina = stod(input.substr(start));
	return data;
}

struct Counter
{
	char pav[10];
	int count;
public:
	int operator++(){ return ++count; }
	int operator--(){ return --count; }
	bool operator==(const Counter &other){ return pav == other.pav; }
	bool operator<(const Counter &other){ return pav < other.pav; }
};

Counter MakeCounter(string line)
{
	Counter counter;
	uint start, end;
	start = 0;
	end = line.find(' ');
	memcpy(counter.pav, line.substr(0, end).c_str(), 10);
	start = end + 1;
	end = line.find(' ', start);
	counter.count = stoi(line.substr(start, end - start));
	return counter;
}

struct Job
{
	bool consume;
	int nr;
}; 

union MessageData
{
	Counter request;
	Data production;
};

struct Message
{
	int code;
	int sender;
	MessageData data;
};

class Buffer
{
	vector<Counter> buffer;
public:
	Buffer(){}
	bool Add(Counter c);
	int Take(Counter c);
	int Size();
	string Print();
	void Done();
};

bool Buffer::Add(Counter c)
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
	//pazadinamas viena duomenu laukianti gija
	return true;
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
void syncOut(vector < vector < Data >> &);
void syncOut(vector < vector < Counter >> &);
string Titles();
string Print(int nr, Data &s);
string Print(Data &data);

int main(int argc, char *argv[])
{
	MPI_Init(&argc, &argv);
	MPI_Finalize();
	return 0;
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
			tmp.push_back(MakeData(lines[i]));
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
			tmp.push_back(MakeCounter(lines[i]));
	}
	return ret;
}


void syncOut(vector < vector < Data >> &data)
{
	cout << setw(3) << "Nr" << Titles() << endl << endl;
	for (unsigned int i = 0; i < data.size(); i++)
	{
		auto &vec = data[i];
		cout << "Procesas_" << i << endl;
		for (unsigned int j = 0; j < vec.size(); j++)
		{
			cout << Print(j, vec[j]) << endl;
		}
	}
}

void syncOut(vector < vector < Counter >> &data)
{
	for (unsigned int i = 0; i < data.size(); i++)
	{
		auto &vec = data[i];
		cout << "Vartotojas_" << i << endl;
		for (unsigned int j = 0; j < vec.size(); j++)
		{
			cout << setw(15) << vec[j].pav << setw(5) << vec[j].count << endl;
		}
	}
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