#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

#define PROMPT ("shell:")
#define MODE ("$")

int main(){
    string path = "";
    string pwd = "~";
    string oldpwd = "";
    bool running = true;

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
        
        //fill command list
        vector<vector<string> > cmdls;
        vector<string> temp;
        cmdls.push_back(temp);
        /*
            Ideas:
                Separate all the strings by pipes and then further by
                    redirection and continue down to quotes and then 
                    spaces
        */
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
                    whitespace = i+1;
                }
                else{
                    cout << "ws:: setting ws to " << i+1 << endl;
                    whitespace = i+1;
                }
            }
            //check for operator (these don't apply in quotes) (these set the whitespace)
            else if(quote == -1 && (input[i] == '|' || input[i] == '<' || input[i] == '>' || input[i] == '&')){
                //don't forget the last string
                if(whitespace != -1 && i > whitespace){
                    cmdls[process].push_back(input.substr(whitespace,i - whitespace));
                }
                process++;
                vector<string> temp;
                cmdls.push_back(temp);
                //operator will exist at start of vector
                cmdls[process].push_back(input.substr(i, 1));
                //next string could exist immediately after
                whitespace = i+1;
            }
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

        //Grammar has been parsed

        for(int i = 0; i < cmdls.size(); i++){
            cout << "process " << i << endl;
            for(int j = 0; j < cmdls[i].size(); j++){
                cout << cmdls[i][j] << endl;
            }
            cout << endl;
        }
        
    }
}
