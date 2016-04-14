#include "Worker.h"
#include <chrono>
#include <sstream>

namespace
{
	std::vector<std::string> 
	splitString(const std::string &s, char delim) 
	{
		std::vector<std::string> elems;
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, delim)) {
			elems.push_back(item);
		}
		return elems;
	}
}

Worker::Worker(const std::string& scoreMatPath):
	_scoreMat(5, 5)
{
	std::ofstream file;
	file.open("results.txt");
	std::chrono::time_point<std::chrono::system_clock> now;
	now = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(now);
	file << "File was produced at: " << std::ctime(&time);
	file << "\n";
	file.close();

	//Parse scoring matrix
	_scoreMat.loadMatrix(scoreMatPath);
}


void Worker::run(const std::string& dataPath, 
				 const std::string& outFormat) 
{
	this->readBubbles(dataPath);
	int prevPercent = -1;
	int counterDone = 0;
	for (auto& bubble : _bubbles)
	{
		++counterDone;
		int percent = 10 * counterDone / _bubbles.size();
		if (percent > prevPercent)
		{
			std::cerr << percent * 10 << "% ";
			prevPercent = percent;
		}

		Record rec;
		std::string prevCandidate = "";
		std::string curCandidate = bubble.candidate;
		outputSeparator();

		while (curCandidate != prevCandidate)
		{
			prevCandidate = curCandidate;
			this->runOneToAll(curCandidate, bubble.branches, rec);
			curCandidate = rec.read;
			if (outFormat == "verbose") 
				outputRecord(rec);
		}

		//Record the rec
		if (outFormat == "short")
			outputRecord(rec);
		outputSeparator();
	}
}

void Worker::runOneToAll(const std::string& candidate, 
						 const std::vector<std::string>& branches,
						 Record& rec) 
{
	double score = 0;
	Alignment align(branches.size());
	//Global
	for (size_t i = 0; i < branches.size(); ++i) 
	{
		score += align.globalAlignment(candidate, branches[i], &_scoreMat, i);
	}

	rec.methodUsed = "global";
	rec.score = score;
	rec.read = candidate;

	//Deletion
	for (size_t del_index = 0; del_index < candidate.size(); del_index++) 
	{
		score = 0;
		for (size_t i = 0; i < branches.size(); i++) {
			score += align.addDeletion(i, del_index + 1);
		}

		//Record if less
		if (score > rec.score) {
			std::string str = candidate;
			rec.methodUsed = "deletion";
			rec.score = score;
			rec.read = str.erase(del_index, 1);
			rec.del_index = del_index;
		}
	}

	//Substitution
	char alphabet[4] = {'A', 'C', 'G', 'T'};
	for (size_t sub_index = 0; sub_index < candidate.size(); sub_index++) 
	{
		for (char letter : alphabet)
		{
			if (letter == candidate[sub_index])
				continue;
			score = 0;

			for (size_t i = 0; i < branches.size(); i++) {
				score += align.addSubstitution(i, sub_index + 1, letter, 
											   branches[i], &_scoreMat);
			}

			//Record if less
			if (score > rec.score) 
			{
				std::string str = candidate;
				rec.methodUsed = "substitution";
				rec.score = score;
				str.erase(sub_index, 1);
				rec.read = str.insert(sub_index, 1, letter);
				rec.sub_index = sub_index;
				rec.sub_letter = letter;
			}
		}
	}

	//Insertion
	for (size_t ins_index = 0; ins_index < candidate.size()+1; ins_index++) 
	{
		for (char letter : alphabet)
		{
			score = 0;
			for (size_t i = 0; i < branches.size(); i++) {
				score += align.addInsertion(i, ins_index + 1, letter, 
											branches[i], &_scoreMat);		
			}

			//Record if less
			if (score > rec.score) 
			{
				std::string str = candidate;
				rec.methodUsed = "insertion";
				rec.score = score;
				rec.read = str.insert(ins_index, 1, letter);
				rec.ins_index = ins_index;
				rec.ins_letter = letter;
			}
		}
	}	
}

void Worker::outputRecord(const Record& rec) 
{
	std::ofstream file;
	file.open("results.txt", std::ios::app);

	file << std::fixed
		 << std::setw(22) << std::left << "Consensus: " 
		 << std::right << rec.read << "\n"
		 << std::setw(22) << std::left << "Score: " << std::right 
		 << std::setprecision(2) << rec.score << "\n"
		 << std::setw(22) << std::left << "Last method applied: " 
		 << std::right << rec.methodUsed << "\n";
	

	if (rec.methodUsed == "deletion") {
		file << "Char at index: " << rec.del_index << " was deleted. \n";
	}
	else if (rec.methodUsed == "substitution") {
		file << "Char at index " << rec.sub_index << " was substituted with " 
			<< "'" << rec.sub_letter << "'" << ".\n";
	}
	else if (rec.methodUsed == "insertion") {
		file << "'"<< rec.ins_letter << "'" 
			 << " was inserted at index " << rec.ins_index << ".\n";
	}
	file << std::endl;
	file.close();
}


void Worker::outputSeparator() 
{
	std::ofstream file;
	file.open("results.txt", std::ios::app);
	file << "------------------------------------------ \n";
	file.close();
}


void Worker::readBubbles(const std::string& fileName)
{
	std::cerr << "Parsing bubbles file\n";
	std::string buffer;
	std::ifstream file(fileName);
	std::string candidate;
	_bubbles.clear();

	if (!file.is_open())
		throw std::runtime_error("Error opening bubble file");

	while (!file.eof())
	{
		std::getline(file, buffer);
		if (buffer.empty())
			break;

		std::vector<std::string> elems = splitString(buffer, ' ');
		if (elems.size() < 3 || elems[0][0] != '>')
			throw std::runtime_error("Error parsing bubbles file");
		std::getline(file, candidate);
		std::transform(candidate.begin(), candidate.end(), 
				       candidate.begin(), ::toupper);
		
		Bubble bubble;
		bubble.candidate = candidate;
		bubble.header = elems[0].substr(1, std::string::npos);
		bubble.position = std::stoi(elems[1]);
		int numOfReads = std::stoi(elems[2]);

		int count = 0;
		while (count < numOfReads) 
		{
			if (buffer.empty())
				break;
			std::getline(file, buffer);
			std::getline(file, buffer);
			std::transform(buffer.begin(), buffer.end(), 
				       	   buffer.begin(), ::toupper);
			bubble.branches.push_back(buffer);
			count++;
		}
		if (count != numOfReads)
			throw std::runtime_error("Error parsing bubbles file");

		_bubbles.push_back(std::move(bubble));
	}
}
