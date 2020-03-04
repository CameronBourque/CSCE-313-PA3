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
#define MODE ("$")
#define HOME_DIR ("/home/")
#define O_OPEN_REDIR_W (O_CREAT | O_WRONLY | O_TRUNC)
#define S_OPEN_REDIR_W (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

void convertPath(string &ret, string path, string home);
void processInput(vector<vector<string> > &cmdls, string &pwd);

int main(){
    //set up global variables
    string path = "";
    string pwd = "~";
    string oldpwd = "";
    bool running = true;
    vector<int> pidList;
    int child = 0;
    string home = HOME_DIR;
    home += getenv("USER");
    
    //set up terminal base
    string newpwd;
    convertPath(newpwd, pwd, home);
    cout << "new pwd: " << newpwd << endl;
    int cret = chdir(newpwd.c_str()); //sets curDir to pwd
    if(cret){
        cout << "directory change failed on init" << endl;
        return 0;
    }
    

    while(true){
        //prompt user
        cout <<  PROMPT << pwd << MODE << " ";
        
        //read input
        string input = "";
        getline(cin, input);
        
        //check for exit
        if(input.compare("exit")==0){
            cout << "Exiting" << endl;
            break;
        }

        //check for cd
        
        //fill command list
        vector<vector<string> > cmdls;
        vector<string> temp;
        cmdls.push_back(temp);
       
        bool doProcessing = true; 
        int quote = -1;
        int tickq = -1;
        int whitespace = 0;
        int process = 0;
        for(int i = 0; i < input.size(); i++){
            //check for end quote
            if(quote > -1){
                if(input[i] == '\"'){
                    cmdls[process].push_back(input.substr(quote, i+1 - quote));
                    quote = -1;
                }
            }
            //check for end tick
            else if(tickq > -1){
                if(input[i] == '\''){
                    cmdls[process].push_back(input.substr(tickq, i+1 - tickq));
                    tickq = -1;
                }
            }
            //check for quotes
            else if(input[i] == '\"'){
                quote = i;
                whitespace = -1;
            }
            //check for ticks
            else if(input[i] == '\''){
                tickq = i;
                whitespace = -1;
            }
            //check for whitespace
            else if(isspace(input[i])){
                if(whitespace > -1 && i > whitespace){
                    cmdls[process].push_back(input.substr(whitespace, i - whitespace));
                }
                
                whitespace = i+1;
            }
            //check for operator (these don't apply in quotes) (these set the whitespace)
            else if(quote == -1 && (input[i] == '|' || input[i] == '<' || input[i] == '>' || input[i] == '&')){
                //don't forget the last string
                if(whitespace != -1 && i > whitespace){
                    cmdls[process].push_back(input.substr(whitespace,i - whitespace));
                }
                //operator will exist at start of vector
                cmdls[process].push_back(input.substr(i, 1));
                
                //add next process space in WHEN PIPING
                if(input[i] == '|'){
                    process++;
                    vector<string> temp;
                    cmdls.push_back(temp);
                }
                
                //make sure valid string from ampersand
                else if(input[i] == '&' && i+1 != input.size()){
                    cout << "Invalid input. Ampersand should be last character" << endl;
                    doProcessing = false;
                }
                
                //next string could exist immediately after
                whitespace = i+1;
            }
            //if at end of string get whatever string hassn't been added
            if(i+1 == input.size()){
                if(quote > -1){
                    cmdls[process].push_back(input.substr(quote, i+1 - quote));
                }
                else if(tickq > -1){
                    cmdls[process].push_back(input.substr(tickq, i+1 - tickq));
                }
                else if(whitespace > -1 && i+1 - whitespace != 0){
                    cmdls[process].push_back(input.substr(whitespace, i+1 - whitespace));
                }
            }
        }

        //remove any empty process
        if(cmdls[cmdls.size()-1].size() == 0){
            cmdls.pop_back();
        }

        //process the input and execute
        if(doProcessing){
            processInput(cmdls, pwd);
        }

        //for debugging purposes
        for(int i = 0; i < cmdls.size(); i++){
            cout << "process " << i << endl;
            for(int j = 0; j < cmdls[i].size(); j++){
                cout << cmdls[i][j] << endl;
            }
            cout << endl;
        }
    }
}

void convertPath(string &ret, string path, string home){
    char buf[path.size() + home.size()-1];

    bool squigle = false;
    for(int i = 0; i < path.size()+home.size()-1; i++){
        if(path[i] == '~'){
            if(squigle){
                buf[i] = path[i];
            }
            else{
                squigle = true;
                strcpy(buf+i, home.c_str());
                i += home.size();
            }
        }
        else{
            buf[i] = path[i];
        }
    }

    ret = buf;
}

void processInput(vector<vector<string> > &cmdls, string &pwd){
    //loop through the processes (all split by pipes)
    for(int p = 0; p < cmdls.size(); p++){
        string last = cmdls[p][cmdls[p].size()-1];
        //check if pipe occurs to another process
        if(last.size() == 1 && last[0] == '|') {
            //FILL
        }
        //if this is the last process
        else{
            //FILL
        }
    }
}

void examples(){
    //THIS IS "ls -l -a > foo.txt" (WRITE TO FILE)
    int fd = open("foo.txt", O_OPEN_REDIR_W, S_OPEN_REDIR_W);
    if(!fork()){
        dup2(fd, 1); //overwrites the stdout with the new file
        execlp ("ls", "ls", "-l", "-a", NULL); //now execute
    }

    //THIS IS "ls -l | grep soda"
    int fds[2];
    pipe(fds);
    if(!fork()){
        dup2(fds[1], 1); //redirect stdout to pipeout
        execlp("ls", "ls", "-l", NULL);
    }
    else{
        dup2(fds[0], 0); //redirect stdin to pipein
        execlp("grep", "grep", "soda", NULL);
    }
}
