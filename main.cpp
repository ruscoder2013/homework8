#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <queue>
#include <algorithm>
#include <thread>
#include <condition_variable>

class threadsafe_queue {
public:
    threadsafe_queue();
    void push(int value) {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(value);
        data_cond.notify_one();
    }

    bool try_pop(int& value) {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return false;
        value = data_queue.front();
        data_queue.pop();
        return true;
    }
    std::shared_ptr<int> try_pop() {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return std::shared_ptr<int>();
        std::shared_ptr<int> res(std::make_shared<int>(data_queue.front()));
        data_queue.pop();
        return res;
    }

    void wait_and_pop(int& value) {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this]() {
            return !data_queue.empty();
        });
        value=data_queue.front();
        data_queue.pop();
    }
    std::shared_ptr<int> wait_and_pop() {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this]() {
            return !data_queue.empty();
        });
        std::shared_ptr<int> res(std::make_shared<int>(data_queue.front()));
        data_queue.pop();
        return res;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
private:
    mutable std::mutex mut;
    std::queue<int> data_queue;
    std::condition_variable data_cond;
};

std::string get_bulk_str(std::vector<std::string>& commands) {
    std::ostringstream oss;
    if (commands.size()==0) return "";
    oss << "bulk: ";
    for(int i = 0; i < commands.size(); i++)
    {
        if (i>0)
            oss << ", ";
        oss << commands[i];
    }
    oss << std::endl;
    return oss.str();
}

std::string time_from_epoch()
{
    std::ostringstream oss;
    using namespace std::chrono;
    system_clock::time_point tp = system_clock::now();
    system_clock::duration dtn = tp.time_since_epoch();
    auto tt = dtn.count() * system_clock::period::num / system_clock::period::den;
    oss << "bulk" << tt << ".log";
    return oss.str();
}

std::string name_file() {
    static std::string prev_name = "";
    std::string name = time_from_epoch();
    if(name.compare(prev_name)==0)
    {
        name += "_1";
    }
    prev_name = name;
    return name;
}

void write_to_file(std::vector<std::string>& commands,
std::queue<std::string>& messages, bool& ready_flag, std::string file_name,
std::ofstream& out) {
    for(int i = 0; i < commands.size(); i++)
        messages.push(commands[i]);
    if (messages.empty()) return;
    out.open(file_name);
    ready_flag = true;
}

void write_to_cout(std::queue<std::string> &message, bool& is_finished) {
    while(!is_finished) {
        while(!message.empty())
        {
            std::string str = message.front();
            std::cout << str;
            message.pop();
        }
    }
}


void write_to_file_1(std::queue<std::string>& messages,
                     bool& is_finished,
                     bool& ready_flag,
                     bool chet, 
                     std::ofstream& out) {
    while(!is_finished) {
        if(ready_flag)
        {
            std::cout << "ready flag" << std::endl;
            while(!messages.empty()) {
                if(chet) {
                    if(messages.size()%2==0) {
                        std::string str = messages.front();
                        std::cout << "thread 1 " << str << std::endl;
                        out << str << std::endl;
                        messages.pop();
                    }
                }
                else {
                    if(messages.size()%2!=0) {
                        std::string str = messages.front();
                        std::cout << "thread 2 " << str << std::endl;
                        out << str << std::endl;
                        messages.pop();
                    }
                }
            }
            ready_flag = false;
            if(out.is_open()) out.close();
        }
    }
}


namespace async {
    struct Handle {
        std::thread *log_thread;
        std::thread *file1_thread;
        std::thread *file2_thread;
        std::condition_variable cv;
        std::mutex mut;
        std::queue<std::string> messages;
        std::queue<std::string> messages_2;
        std::vector<std::string> commands;
        std::chrono::milliseconds ms;
        std::string string_buffer;
        std::string cmd;
        std::string file_name; 
        int brace_count = 0;
        int handle = 1;
        bool finish = false;
        bool ready_flag = false;
        int N;
        std::ofstream out;
        ~Handle() {
            if(log_thread!=nullptr)
                delete log_thread;
            if(file1_thread!=nullptr)
                delete file1_thread;
            if(file2_thread!=nullptr)
                delete file2_thread;
        }
    };
    using handle_t = Handle*;
    std::vector<handle_t> handles;
    
    void write_to_cout2(Handle &handle) {
        std::unique_lock<std::mutex> lk(handle.mut);
        while(!handle.finish) {
            handle.cv.wait(lk, [&handle](){ 
                return !handle.messages.empty() || handle.finish; 
                });
            while(!handle.messages.empty())
            {
                std::string str = handle.messages.front();
                std::cout << str;
                handle.messages.pop();
            }
            if(handle.finish)
                break; 
        }
    }

    handle_t connect(std::size_t bulk) {
        handle_t handle = new Handle();
        //handle->log_thread = new std::thread(write_to_cout, std::ref(handle->messages), std::ref(handle->finish));
        handle->N = bulk;
        handle->log_thread = new std::thread(async::write_to_cout2, std::ref(*handle));
        
        /*handle->file1_thread = new std::thread(write_to_file_1, std::ref(handle->messages_2), std::ref(handle->finish),
        std::ref(handle->ready_flag), true, std::ref(out));
        handle->file2_thread = new std::thread(write_to_file_1, std::ref(handle->messages_2), std::ref(handle->finish),
        std::ref(handle->ready_flag), false, std::ref(out));*/
        handles.push_back(handle);
        return handle;
    }

    

    void try_to_show(handle_t handel) {
        if(handel->brace_count==0) {
            auto str = get_bulk_str(handel->commands);
            handel->messages.push(str);
            handel->cv.notify_one();
            //std::cout << "push" << std::endl;
            /*write_to_file(handel->commands, 
                      handel->messages_2, 
                      handel->ready_flag, 
                      handel->file_name, 
                      handel->out);*/
            handel->commands.clear();
        }
        
    }

    void receive(handle_t handler, const char* data, std::size_t size) {
        std::string s(data);
        if(handler->string_buffer.size()!=0)
        {
            s = handler->string_buffer + s;
            handler->string_buffer.clear();
        }
        std::stringstream ss(s);
        std::string cmd;
        std::vector<std::string> elems;
        while (std::getline(ss, cmd)) {
            if(ss.eof()) continue;
            //std::cout << cmd << std::endl;
            if(cmd.compare("{")==0) {
                try_to_show(handler);
                handler->brace_count++;
            } else if(cmd.compare("}")==0) {
                handler->brace_count--;
                try_to_show(handler);
                if(handler->brace_count<0) {
                    handler->brace_count++;
                    continue;
                }
            } else {
                if (handler->commands.size()==0)
                    handler->file_name = name_file();
                handler->commands.push_back(cmd);
                if(handler->commands.size()>=handler->N)
                    try_to_show(handler);
            }
        }
        if(data[size-1]!='\n')
            handler->string_buffer = cmd;
        //try_to_show(handler);
    }
    void disconnect(handle_t handler) {
        std::vector<handle_t>::iterator missing = std::find(handles.begin(), handles.end(), handler);
        if(missing!=handles.end())
        {
            try_to_show(handler);
            handles.erase(missing);
            handler->finish = true;
            handler->cv.notify_one();
            handler->log_thread->join();
            /*handler->file1_thread->join();
            handler->file2_thread->join();*/
            delete handler;
        }
    }
}


int main(int argc, char *argv[]) {
    auto handler = async::connect(3);
    async::receive(handler, "cmd1\n", 5);
    async::receive(handler, "cmd2\n", 5);
    async::receive(handler, "cmd3\ncmd4\ncmd", 14);
    async::receive(handler, "5\n", 3);
    async::disconnect(handler);
    return 0;
}