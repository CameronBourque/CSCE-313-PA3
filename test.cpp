#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>

using namespace std;

#define TRIM (" \t")
#define PROMPT ("shell:")
#define PROMPT_POST ("$")
#define HOME_DIR ("/home/")
#define O_OPEN_REDIR (O_CREAT | O_WRONLY | O_TRUNC)
#define S_OPEN_REDIR (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define BUF_SIZE (1024)

//                                            HELPER METHODS
void changeDirectories(string home, string newpwd, string &pwd, string &oldpwd){
    //if going to previous directory
    if(newpwd.find("-") == 0 && newpwd.size() == 1){
        string temp = oldpwd;
        if(temp.find("~") == 0){
            temp.replace(0, 1, home);
        }
        chdir(temp.c_str());
        temp = oldpwd;
        oldpwd = pwd;
        pwd = temp;
    }
        //else try to go to input directory
    else{
        //try to change to directory
        if(!chdir(newpwd.c_str())){
            char cwd[BUF_SIZE];
            getcwd(cwd, BUF_SIZE);
            newpwd = cwd;
            if(newpwd.find(home) == 0){
                newpwd.replace(0, home.size(), "~");
            }
            oldpwd = pwd;
            pwd = newpwd;
        }
        //else input directory was invalid
        else{
            cout << "Failed to change directories" << endl;
        }
    }
}

int execute(char** args, bool pipeps){
    int fds[2];
    if(pipe){
        pipe(fds);
    }

    int ret = fork();
    if(ret == -1){
        cout << "Failed to create child process" << endl;
    }
    else if(!ret){
        execvp(args[0], args);
    }
    return ret;

}

int processRedirection(char** args, string* in, int inSize, int index, vector<int> &pidList, int &child, bool pipeps){

    //TODO FILL

    return -1;
}

bool processInput(vector<string> input, vector<int> &pidList, int &child){
    int stdoutfd = dup(1); //need a backup of stdout
    int stdinfd = dup(0);  //need a backup of stdin

    for(int i = 0; i < input.size(); i++){
        int baseIn = input[i].find_first_not_of(TRIM);
        input[i] = input[i].substr(baseIn, input[i].find_last_not_of(TRIM) - baseIn+1);

        //DEBUG ---- TODO REMOVE LATER
        cout << input[i] << endl;

        int ws = 0;
        int sIndex = 0; //index in each arg
        int quote = -1;
        int tick = -1;

        //get an array of all arguments and operators so we can process everything correctly
        string* sArgs = new string[input[i].size()];
        for(int j = 0; j < input[i].size(); j++){
            //priority is always quotes and ticks
            if(input[i][j] == '\"' && tick == -1){
                if(quote == -1){
                    quote = j;
                    ws = quote; //this prevents problems when missing closing quote
                }
                else{
                    sArgs[sIndex++] = input[i].substr(quote, j+1-quote);
                    ws = j+1;
                    quote = -1;
                }
            }
            else if(input[i][j] == '\'' && quote == -1){
                if(tick == -1){
                    tick = j;
                    ws = tick; //this prevents problems when missing closing tick
                }
                else{
                    sArgs[sIndex++] = input[i].substr(tick, j+1-tick);
                    ws = j+1;
                    tick = -1;
                }
            }
            //separate every argument by space as secondary goal
            else if(isspace(input[i][j]) && quote == -1 && tick == -1){
                if(ws != j){ //we don't want whitespace in any argument
                    sArgs[sIndex++] = input[i].substr(ws, j-ws);
                }
                ws = j+1;
            }
            //if we find any operator '<' or '>' then split by that and store operator
            else if((input[i][j] == '<' || input[i][j] == '>') && quote == -1 && tick == -1){
                //treat like a whitespace to capture any previous argument
                if(ws != j){
                    sArgs[sIndex++] = input[i].substr(ws, j-ws);
                }
                ws = j+1;

                //store operator
                sArgs[sIndex++] = input[i].substr(j, 1);
            }
            //if '&' is not last character then ignore and let execution fail
            else if(input[i][j] == '&' && j+1 == input[i].size() && i+1 == input.size()){
                //treat like a whitespace to capture any previous argument
                if(ws != j){
                    sArgs[sIndex++] = input[i].substr(ws, j-ws);
                }
                ws = j+1;

                //store operator
                sArgs[sIndex++] = input[i].substr(j, 1);
            }
        }

        //get any leftover arguments
        if(ws != input[i].size()){
            sArgs[sIndex++] = input[i].substr(ws);
        }

        //DEBUG ---- TODO REMOVE LATER
        for(int j = 0; j < sIndex; j++){
            cout << "[START:" << sArgs[j] << ":END]" << endl;
        }
        cout << "exe list: " << endl;

        //fill the execution list
        char** args = new char*[BUF_SIZE];
        int argsIndex = 0;
        int j;
        for(j = 0; j < sIndex; j++){
            if(sArgs[j].compare("<") == 0 || sArgs[j].compare(">") == 0 || sArgs[j].compare("&") == 0){
                break;
            }
            else{
                char* temp = new char[BUF_SIZE];
                strcpy(temp, sArgs[j].c_str());
                args[argsIndex++] = temp;
                //DEBUG ---- TODO REMOVE LATER
                cout << sArgs[j] << ", ";
            }
        }
        args[argsIndex++] = NULL;
        //DEBUG ---- TODO REMOVE LATER
        cout << "\\0" << endl;

        bool pipeps = i+1 != input.size();

        //processes redirections
        if(sArgs[j].compare("<") == 0 || sArgs[j].compare(">") == 0){
            //this could be recursive
            int ret = processRedirection(args, sArgs, sIndex, j, pidList, child, pipeps);

            //execution failure safety
            if(!ret){ return false; }
            //fork failure safety
            else if(ret == -1){ return true; }
        }
        //execute the arguments
        else{
            //see if need to run in background or not
            if(sArgs[j].compare("&") == 0){
                int pid = execute(args, pipeps);

                //execution failure safety
                if(!pid){ return false; }
                //fork failure safety
                else if(pid == -1){ return true; }

                //store child pid
                pidList.push_back(pid);
            }
            else{
                child = execute(args, pipeps);

                //execution failure safety
                if(!child){ return false; }
                //fork failure safety
                else if(child == -1){ child = 0; return true; }

                //wait on child to terminate
                wait(&child);
            }
        }

        //delete what was allocated on heap
        delete [] sArgs;
        delete [] args;

#if 0
        char* args[processes[i].size()];
        int ws = 0;
        int index = 0;
        string filename = "";
        for(int j = 0; j < processes[i].size(); j++){
            if(isspace(processes[i][j])){
                if(ws != j){
                    char* temp = new char[j-ws+1];
                    strcpy(temp, processes[i].substr(ws, j-ws).c_str());
                    args[index++] = temp;
                }
                ws = j+1;
            }
        }
        char* temp = new char[processes[0].size()-ws+1];
        strcpy(temp, processes[i].substr(ws).c_str());
        args[index++] = temp;
        args[index] = NULL;

        //fork out child to execute with arguments
        child = fork();
        if(!child){
            execvp(args[0], args);
            cout << "Failed to execute command " << args[0] << endl;
            running = false;
        }
        else{
            //wait on child process
            wait(&child);

            //remove memory leaks
            for(int k = 0; args[k] != NULL; k++){
                delete [] args[k];
            }
        }
#endif
    }

    //reattach stdout and stdin
    dup2(1, stdoutfd);
    dup2(0, stdinfd);

    return true;
}

//                                                  MAIN METHOD
int main(){
    //set up variables for keeping track of important things
    string pwd = "~";
    string oldpwd = "";
    bool running = true;
    vector<int> pidList;    //this and child could also be pid_t
    int child = 0;
    string home = HOME_DIR;     //this stores the string equivalent of ~ for conversion purposes
    home += getenv("USER");

    //set up terminal at home directory
    chdir(home.c_str());

    //for immersion
//    printf("\033[2J\033[3J");
    
    //go until shell exits
    while(running){
        //prompt input from user
        cout << PROMPT << pwd << PROMPT_POST << " ";

        //read input
        string input = "";
        getline(cin, input);
        int baseIn = input.find_first_not_of(TRIM);
        input = input.substr(baseIn, input.find_last_not_of(TRIM) - baseIn+1);

        //check for exit
        if(input.compare("exit") == 0){
            cout << "Exiting" << endl;
            break;
        }

        //check for cd  
        int cdLocation = input.find("cd ");
        if(cdLocation == 0){
            //get string of just the directory
            string newpwd = input.substr(3);
            int dirStart = newpwd.find_first_not_of(" \t");
            newpwd = newpwd.substr(dirStart);

            changeDirectories(home, newpwd, pwd, oldpwd);

            continue;
        }
        

        //fill vector of processes (these are separated by pipes outside of quotes and ticks)
        vector<string> processes;
        int shdw = 0;
        int quote = -1;
        int tick = -1;
        for(int i = 0; i < input.size(); i++){
            if(input[i] == '\"' && tick == -1){
                if(quote == -1){
                    quote = i;
                }
                else{
                    quote = -1;
                }
            }
            else if(input[i] == '\'' && quote == -1){
                if(tick == -1){
                    tick = i;
                }
                else{
                    tick = -1;
                }
            }
            if(input[i] == '|' && quote == -1 && tick == -1){
                processes.push_back(input.substr(shdw, i-shdw));
                shdw = i+1;
            }
        }
        if(input.size() != shdw) {
            processes.push_back(input.substr(shdw));
        }

        running = processInput(processes, pidList, child);
    }

    //for immersion
//    printf("\033[2J\033[3J");
}
