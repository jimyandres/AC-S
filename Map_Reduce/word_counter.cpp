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

struct membuf : std::streambuf
{
    membuf(char* begin, char* end) {
        this->setg(begin, begin, end);
    }
};

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

std::string getNextToken_parallel(std::istream &in, int end)
{
    char c;
    std::string ans="";
    c=in.get();
    while(!std::isalnum(c) && !(in.tellg() != end)) { //remove non letter charachters
        mtx.lock();
        std::cout << "Non letter: " << c << std::endl;
        c=in.get();
        mtx.unlock();
    }
    while(std::isalnum(c)) {
        mtx.lock();
        std::cout << "Letter: " << c << std::endl;
        ans.push_back(std::tolower(c));
        c=in.get();
        mtx.unlock();        
    }
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

void countWords_parallel(std::string file, int begin, int end)
{
    mtx.lock();
    std::cout << "thread: " << std::this_thread::get_id() << " Reading Range: (" << begin << ", " << end << ")" << std::endl;
    mtx.unlock();
    // std::ifstream in(file);
    // in.seekg(begin, in.beg);
    // std::string s;
    // std::string empty ="";
    // while((s=getNextToken_parallel(in, end))!=empty ) {
    //     mtx.lock();
    //     ++words[s];
    //     std::cout << "thread: " << std::this_thread::get_id() << " Read: " << s << std::endl;
    //     mtx.unlock();
    //     //cout << "thread: " << this_thread::get_id() << " read: " << s << endl;
    // }
}

void showResults()
{
    for (auto& x: words)
        std::cout << x.first << ": " << x.second << std::endl;
}

int main()
{
    std::ifstream fin("input.txt", std::ifstream::binary);

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
            batch_size += (size - begin);

        int end = begin + batch_size;
        std::cout << "Begin: " << begin << std::endl;
        if(end > size)
            end = size;
        fin.seekg(end, fin.beg);
        std::cout << "Pos: " << fin.tellg() << std::endl;
        char c;
        c=fin.get();
        while(!isspace(c) && !fin.eof()) {
            end++;
            c=fin.get();
        }
        std::cout << "End: " << end << std::endl;
        fin.seekg(begin, fin.beg);
        std::cout << "Pos: " << fin.tellg() << std::endl;
        char * data = new char [(end - begin)];
        fin.read(data, (end - begin));

        membuf sbuf(data, data + sizeof(data));
        std::istream in(&sbuf);
        std::cout << "Data: " << data << std::endl;
        // in.get(std::cout);
        delete[] data;
        //threads.push_back(std::thread(countWords_parallel, "input.txt", begin, end));
        begin += (end - begin);
    }
    std::cout << "synchronizing all threads..." << std::endl;
    for (auto& th : threads) {
        if(th.joinable()) {
            th.join();
        }
    }

    showResults(); 
}

/*#include <iostream>
#include <istream>
#include <streambuf>
#include <string>

struct membuf : std::streambuf
{
    membuf(char* begin, char* end) {
        this->setg(begin, begin, end);
    }
};

int main()
{
    char buffer[] = "I'm a buffer with embedded nulls\0and line\n feeds";

    membuf sbuf(buffer, buffer + sizeof(buffer));
    std::istream in(&sbuf);
    std::string line;
    while (std::getline(in, line)) {
        std::cout << "line: " << line << "\n";
    }
    return 0;
}*/