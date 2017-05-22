#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <cctype>
#include <thread>
#include <vector>
#include <sstream>

std::string getNextToken(std::istream &in)
{
    char c;
    std::string ans="";
    c=in.get();
    while(!std::isalnum(c) && !in.eof()) { //remove non letter charachters
        c=in.get();
    }
    while(std::isalnum(c)) {
        ans.push_back(std::tolower(c));
        c=in.get();
    }
    return ans;
}

void countWords(std::string data, std::unordered_map<std::string, int> &words)
{
    std::istringstream in(data);
    std::string s;
    std::string empty ="";
    while((s=getNextToken(in))!=empty ) {
        ++words[s];
    }
}

int search_key(std::unordered_map<std::string, int> &words, std::string key)
{
    return words[key];
}

void showResults(std::unordered_map<std::string, int> &words)
{
    for (auto& x: words)
        std::cout << x.first << ": " << x.second << std::endl;
}

std::vector<std::string> getOptions(std::string input)
{
    std::vector<std::string> inputs;
    std::istringstream f(input);
    std::string tmp;   
    int i =0;
    while (getline(f, tmp, ' ') && i < 2) {
        inputs.push_back(tmp);
        i++;
    }
    return inputs;
}

void printMenu()
{
    std::cout << "\n\n***********************************\n";
    std::cout << "Enter action to perform: \n\tsearch <word>\n\t(sh) Show all results\n\t(ex) Exit\n";
    std::cout << "\n";
}

void input_handler(std::unordered_map<std::string, int> &words, std::string op, std::string val)
{
    if(op == "search") {
        std::cout << "\n\n";
        std::cout << val << ": " << search_key(words, val) << std::endl;
    } else if(op == "sh") {
        showResults(words);
    } else {
        std::cout << "Unkown option" << std::endl;
    }
}

int main(int argc, char** argv)
{
    if(argc != 3) {
        std::cout << "Usage: ./word_counter <file_path> <nthreads>" << std::endl;
        return -1;
    }
    std::ifstream fin(argv[1]);
    int nthreads = atoi(argv[2]);

    std::vector<std::thread> threads(nthreads);
    std::vector<std::unordered_map<std::string,int>> local_hash(nthreads);

    fin.seekg(0, fin.end);
    int size = fin.tellg();
    fin.seekg(0, fin.beg);

    int begin = 0;
    int batch_const = size/nthreads;
    for(int i=0; i<nthreads; i++) {
        int batch_size;
        if(i == nthreads - 1)
            batch_size = (size - begin);
        else
            batch_size = (batch_const * (i+1)) - begin;
        int end = begin + batch_size;
        if(end > size)
            end = size;
        fin.seekg(batch_size, fin.cur);
        char c;
        if(fin.tellg() != size) {
            c=fin.peek();
            while(!fin.eof() && !isspace(c)) {
                c=fin.get();
                end++;
                c=fin.peek();
            }
        }
        fin.seekg(-(end-begin), fin.cur);
        char * data = new char [(end - begin)];
        fin.read(data, (end - begin));
        std::string tmp(data, (end-begin));
        threads.push_back(std::thread(countWords, tmp, std::ref(local_hash[i])));
        data = NULL;
        delete[] data;
        begin = end;
    }
    for (auto& th : threads) {
        if(th.joinable()) {
            th.join();
        }
    }
    std::unordered_map<std::string,int> words;
    for(auto& local: local_hash) {
        for(auto& word: local) {
            words[word.first] += word.second;
        }
    }

    std::cout << "Finished reading book!!\n" << std::endl;
    while(true) {
        printMenu();
        std::string val;
        std::vector<std::string> inputs;
        getline(std::cin, val);
        if(val != "") {
            inputs = getOptions(val);
            if(inputs.front() == "q" || inputs.front() == "quit" || inputs.front() == "Quit" || inputs.front() == "ex" || inputs.front() == "Exit" || inputs.front() == "exit") {
                break;
            }
            if(inputs.size() < 2 && inputs.front() != "sh") {
                std::cout << "Missing argument!" << std::endl;
            } else {
                input_handler(words, inputs.front(), inputs.back());
                //cout << "Input is: " << val << endl;
            }
        }
    }
    return 0;
}