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

#define PROMPT ("shell:")
#define PROMPT_POST ("$")
#define HOME_DIR ("/home/")
#define O_OPEN_REDIR (O_CREAT | O_WRONLY | O_TRUNC)
#define S_OPEN_REDIR (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

int main(){
    //set up variables for keeping track of important things
    string pwd = "~";
    string oldpwd = "";
    bool running = true;
    vector<int> pidList;
    int child = 0;
    string home = HOME_DIR;     //this stores the equivalent of ~
    home += getenv("USER");

    //set up terminal base
    chdir(home.c_str());
    
    //go until shell exits
    while(running){
        //prompt input from user
        cout << PROMPT << pwd << PROMPT_POST << " ";

        //read input
        string input = "";
        getline(cin, input);
        int baseIn = input.find_first_not_of(" \t");
        input = input.substr(baseIn, input.find_last_not_of(" \t") - baseIn+1);

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
                    char cwd[512];
                    getcwd(cwd, 512);
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
            continue;
        }
        

        //fill vector of processes
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
                processes.push_back(input.substr(shdw, i));
                shdw = i+1;
            }
        }
        processes.push_back(input.substr(shdw, input.size()-shdw));

        //MAY NEED TO DO THIS RECURSIVELY
        //parse input arguments and execute
        for(int i = 0; i < processes.size(); i++){
            cout << processes[i] << endl;
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
                else if(processes[i][j] == '<' || processes[i][j] == '>'){
                    if(ws != j){
                        char * temp = new char[j-ws+1];
                        strcpy(temp, processes[i].substr(ws, j-ws).c_str());
                    }
                    
                    j++;
                    ws = j;
                    for(; j < processes[i].size(); j++){
                        if(isspace(processes[i][j])){
                            if(ws != j){
                                filename = processes[i].substr(ws, j-ws);
                                break;
                            }
                            else{
                                ws = j+1;
                            }
                        }
                    }
                    if(j == processes[i].size()){
                        filename = processes[i].substr(ws);
                    }
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
        }
    }
}