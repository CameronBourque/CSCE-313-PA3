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
#define O_REDIR_O (O_CREAT | O_WRONLY | O_TRUNC)
#define O_REDIR_I (O_RDONLY)
#define S_REDIR (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define BUF_SIZE (1024)

//THIS IS FOR CHANGING DIRECTORIES AFTER USER INPUTS "cd ..."
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

//THIS EXECUTES A LIST OF ARGUMENTS AND PIPES THEIR OUTPUT IF NEEDED
int execute(char** args){
    int ret = fork();
    if(ret == -1){
        cout << "Failed to create child process" << endl;
    }
    else if(!ret){
        execvp(args[0], args);
    }
    return ret;
}

//THIS PROCESSES REDIRECTION WHICH WILL REPLACE STDIN AND STDOUT
bool processRedirection(char** args, string* in, int inSize, int index, int &fileIn, int &fileOut){
    //check for invalid input
    if(index+1 == inSize){ cout << "Invalid input" << endl; return false; }

    //if reading from file
    if(in[index].compare("<") == 0){
        //need to close out old input file
        try{ close(fileIn); }
        catch(int e){}

        //try to open the file
        string filename = in[++index];
        fileIn = open(filename.c_str(), O_REDIR_I, S_REDIR);

        //handle open failure
        if(fileIn == -1){ cout << "Failed to open file: " << filename << endl; return false; }

        //make file the stdin
        dup2(fileIn, 0);
    }
    //else if writing to file
    else if(in[index].compare(">") == 0){
        //need to close out old output file
        try{ close(fileOut); }
        catch(int e){}

        //try to open the file
        string filename = in[++index];
        fileOut = open(filename.c_str(), O_REDIR_O, S_REDIR);

        //handle open failure
        if(fileOut == -1){ cout << "Failed to open file: " << filename << endl; return false; }

        //make file the stdin
        dup2(fileOut, 1);
    }
    else{
        cout << "A bug caused the code to reach this point" << endl;
        return false;
    }

    //see if need to keep doing redirection
    index++;
    if(index < inSize && (in[index].compare("<") == 0 || in[index].compare(">") == 0)){
        return processRedirection(args, in, inSize, index, fileIn, fileOut);
    }

    //return successful file handling on final redirection
    return true;
}

//THIS ORGANIZES INPUT VALUES TO BE EXECUTED PROPERLY
bool processInput(vector<string> input, vector<int> &pidList){
    int stdoutfd = dup(1); //need a backup of stdout
    int stdinfd = dup(0);  //need a backup of stdin

    //create array for file descriptors
    int fds[2];

    //each process should pipe into the next unless a redirection overrides it
    for(int i = 0; i < input.size(); i++){
        int baseIn = input[i].find_first_not_of(TRIM);
        input[i] = input[i].substr(baseIn, input[i].find_last_not_of(TRIM) - baseIn+1);

        //DEBUG FOR PIPES ---- TODO REMOVE LATER
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

        //fill the execution list
        char** args = new char*[BUF_SIZE];
        int argsIndex = 0;
        int j;
        for(j = 0; j < sIndex; j++){
            //the arguments exist before any operators
            if(sArgs[j].compare("<") == 0 || sArgs[j].compare(">") == 0 || sArgs[j].compare("&") == 0){
                break;
            }
            //add all arguments to the list
            else{
                char* temp = new char[BUF_SIZE];
                strcpy(temp, sArgs[j].c_str());
                args[argsIndex++] = temp;
            }
        }
        //add the null value to the end of the execution list
        args[argsIndex++] = NULL;

        //see if need to pipe exists
        if(i+1 < input.size()){
            //make sure the pipe works
            if(pipe(fds) == -1){
                cout << "Failed to create pipe. Not executing." << endl;

                //close the fds
                try {
                    close(fds[0]);
                }
                catch(int e){}
                try{
                    close(fds[1]);
                }
                catch(int e){}

                //delete heap instantiations
                delete [] sArgs;
                delete [] args;

                //continue without executing
                return true;
            }
        }

        //redirection will override the pipe
        bool redirection = false;

        //processes redirections
        if(sArgs[j].compare("<") == 0 || sArgs[j].compare(">") == 0){
            //this could be recursive
            redirection = processRedirection(args, sArgs, sIndex, j, fds[0], fds[1]);

            //on failure to redirect
            if(!redirection){
                //close the fds
                try {
                    close(fds[0]);
                }
                catch(int e){}
                try{
                    close(fds[1]);
                }
                catch(int e){}

                //reattach stdout and stdin
                dup2(stdoutfd, 1);
                dup2(stdinfd, 0);

                //delete heap instantiations
                delete [] sArgs;
                delete [] args;

                //continue without executing
                return true;
            }
        }

        //get the child pid after forking
        int pid = fork();

        //this will handle the child process and any forking error
        if(!pid || pid == -1){
            //try to execute the child
            if(!pid){
                //replace the stdout to next process
                dup2(fds[1], 1);

                //execute
                execvp(args[0], args);
            }
            //print out error for fork failure if that is what happened
            else{
                cout << "Failed to create child process" << endl;
            }

            //close fds
            try{ close(fds[0]); }
            catch(int e){}
            try{ close(fds[1]); }
            catch(int e){}

            //reattach stdout and stdin
            dup2(stdoutfd, 1);
            dup2(stdinfd, 0);

            //child process should not duplicate parent
            if(!pid){ return false; }

            //delete heap instantiations
            delete [] sArgs;
            delete [] args;

            //continue without execution
            return true;
        }
        //this will handle the parent process
        else{
            //replace the stdin for next process
            dup2(fds[0], 0);

            //close the stdout to prevent any waits
            close(fds[1]);
        }

        //see if need to run in background or not
        if(j < sIndex && sArgs[j].compare("&") == 0){
            //store child pid
            pidList.push_back(pid);
        }
        else{
            //TODO THIS NEEDS A REWORK
            //wait on child to terminate TODO need to fix for only the last level of pipes
            if(i+1 == input.size()) {
                waitpid(pid, 0, 0);
            }
        }

        //delete what was allocated on heap
        delete [] sArgs;
        delete [] args;
    }

    //close the fds
    try {
        close(fds[0]);
    }
    catch(int e){}
    try{
        close(fds[1]);
    }
    catch(int e){}

    //reattach stdout and stdin
    dup2(stdoutfd, 1);
    dup2(stdinfd, 0);

    return true;
}

//THIS IS THE MAIN LOOP OF THE WHOLE PROGRAM AND KEEPS TRACK OF THE IMPORTANT VARIABLES
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

        //prevent segfault
        if(input.size() == 0){ continue; }

        //trim leading and trailing whitespace
        int baseIn = input.find_first_not_of(TRIM);
        input = input.substr(baseIn, input.find_last_not_of(TRIM) - baseIn+1);

        //check for exit
        if(input.compare("exit") == 0){
            cout << "Exiting" << endl;
            break;
        }

        //check for jobs
        if(input.compare("jobs") == 0){
            int count = 1;
            for(int i = 0; i < pidList.size(); i++){
                //determine if child is running
                int pid = waitpid(pidList[i], 0, WNOHANG);
                if(pid){
                    pidList.erase(pidList.begin() + i);
                    i--;
                    cout << "[" << count << "]\t" << "Done" << endl;
                }
                else{
                    cout << "[" << count << "]\t" << "Running" << endl;
                }
                count++;
            }

            continue;
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

        //try to execute
        running = processInput(processes, pidList);

        //check for background processes
        int count = 1;
        for(int i = 0; i < pidList.size(); i++){
            int pid = waitpid(pidList[i], 0, WNOHANG);
            if(pid){
                pidList.erase(pidList.begin() + i);
                i--;
                cout << "[" << count << "]\t" << "PID: " << pid << "\tDone" << endl;
            }
            count++;
        }
    }

    //for immersion
//    printf("\033[2J\033[3J");
}
