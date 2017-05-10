#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <cctype>
#include <thread>
#include <mutex>
#include <vector>

std::unordered_map<std::string,int> words;
std::mutex mtx;

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

std::string getNextToken_parallel(std::istream &in, int begin, int end)
{
    // mtx.lock();
    in.seekg(begin, in.beg);
    char c;
    std::string ans="";
    c=in.get();
    while(!std::isalnum(c) && !(in.tellg() != end)) { //remove non letter charachters
        c=in.get();
    }
    while(std::isalnum(c)) {
        ans.push_back(std::tolower(c));
        c=in.get();
    }
    // mtx.unlock();
    return ans;
}

void countWords(std::istream &in)
{
    std::string s;
    std::string empty ="";
    while((s=getNextToken(in))!=empty ) {
        ++words[s];
    }
}

void countWords_parallel(std::istream &in, int begin, int end)
{
    // mtx.lock();
    // std::cout << "thread: " << std::this_thread::get_id() << " Reading Range: (" << begin << ", " << end << ")" << std::endl;
    // mtx.unlock();
    std::string s;
    std::string empty ="";
    while((s=getNextToken_parallel(in, begin, end))!=empty ) {
        // mtx.lock();
        ++words[s];
        // mtx.unlock();
        //cout << "thread: " << this_thread::get_id() << " read: " << s << endl;
    }
}

void showResults()
{
    for (auto& x: words)
        std::cout << x.first << ": " << x.second << std::endl;
}

int main()
{
    std::ifstream fin("input.txt");

    // countWords(fin);
    // showResults();
    // words.clear();
    // fin.close();

    // fin.open("input.txt");
    int nthreads = 2;
    std::vector<std::thread> threads;
    fin.seekg(0, fin.end);
    int size = fin.tellg();
    fin.seekg(0, fin.beg);

    int begin = 0;
    int batch_size = size/nthreads;
    for(int i=0; i<nthreads; i++) {
        if(i == nthreads - 1)
            batch_size += (size - (batch_size*nthreads));
        threads.push_back(std::thread(countWords_parallel, std::ref(fin), begin, begin + batch_size));
        begin += batch_size;
    }
    std::cout << "synchronizing all threads..." << std::endl;
    for (auto& th : threads) {
        if(th.joinable()) {
            th.join();
        }
    }

    showResults(); 
}