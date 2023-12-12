#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <map>

using namespace std;

void parse_and_run_command(const std::string &command) {
    /* TODO: Implement this. */
    /* Note that this is not the correct way to test for the exit command.
       For example the command "   exit  " should also exit your shell.
    */
    // vector of strings will contain each file, operation, or argument in its own index

    // REMOVING ALL WHITESPACE AND FORMATTING EACH WORD
    vector<string> strings = {};
    string curWord = "";
    int status;
    for(int i=0; i < (int)command.length(); i++){
        if (command[i] == ' ' || command[i] == '\t' || command[i] == '\v') {
            if(curWord != ""){ strings.push_back(curWord); }
            curWord = "";
        } else if (i == (int)command.length()-1) {
            curWord += command[i];
            strings.push_back(curWord);
        } else { curWord += command[i]; }
    }
    // change the vector of strings to format that can be passed into exec
    const char **argv = new const char* [strings.size()+1];
    map <int, const char**> commands;

    // keep track of important variables for later
    vector<int> count_length_vec;
    vector<string> input_files;
    vector<string> output_files;
    
    // parsing and counter to track for malformed commands
    int count_input = 0;
    int count_output = 0;
    int count_length = 0;
    string input_filename = "";
    string output_filename = "";
    int num_commands = 0;

    // HANDLING ALL PARSING OF TOKENS AND WORDS (TURNING INTO PROPER COMMANDS)
    // Iterate through and check if malformed, and also prepare output/input redirections.
    for(int i=0; i < (int)strings.size(); i++){
        // Handling Pipes
        if(strings[i] == "|"){
            // storing old variables
            count_length_vec.push_back(count_length);
            input_files.push_back(input_filename);
            output_files.push_back(output_filename);

            char **argv_copy = new char* [count_length+1];
            for(int j=0; j < count_length; j++){
                // need to copy contents of argv into argv_copy
                argv_copy[j] = new char [strlen(argv[j])]; 
                argv_copy[j] = (char*) argv[j];
                // set argv[j] to an empty string
                argv[j] = "";
            }
            commands[num_commands] = (const char **) argv_copy;
            
            // resetting counters for new commands
            count_input = 0;
            count_output = 0;
            count_length = 0;
            input_filename = "";
            output_filename = "";
            num_commands++;
        }
        
        // I/O Redirection
        else if (strings[i] == "<" || strings[i] == ">"){
            // check that it is not an operator or empty and that we are not out of bounds
            // if a proper string
            if (i+1 < (int)strings.size() && strings[i+1] != "<" && strings[i+1] != ">" && strings[i+1] != ""){ //check if piping later
                // keep track of filename
                // specify if input or output char
                // if input redirection
                if (strings[i] == "<"){
                    count_input++;
                    if (count_input > 1){
                        std::cerr << "invalid command.\n";
                    } else {
                        input_filename = strings[i+1];
                        i++;
                    }
                } else if (strings[i] == ">") {
                    count_output++;
                    if (count_output > 1) {
                        std::cerr << "invalid command.\n";
                    } else {
                        output_filename = strings[i+1];
                        i++;
                    }
                }
            // if we have a redirection token followed by something other than a word
            } else {
                std::cerr << "invalid command. (Input token followed by non-word)\n";
            }
        } 
        else {
            // really stupid we were setting argv[i] equal to string[1]. This would only work if command came first
            // we need to be setting it to be count_length instead otherwise we were getting like: "< Makefile /bin/cat > output.txt"
            // since "/bin/cat" is at strings[3] we were setting argv[3] to it and then setting argv[1] to null making argv[0] also null
            // when we were printing all of argv[i] we could not tell we should have printed the iterator next to it to see what index the output was in bc the rest printed null
            argv[count_length] = strings[i].c_str();
            count_length++;
        }
        
        // if at the end of the command inputted add it to the commands array
        if(i+1 == (int)strings.size()){
            // storing old variables
            count_length_vec.push_back(count_length);
            input_files.push_back(input_filename);
            output_files.push_back(output_filename);
            commands[num_commands] = argv;
            num_commands++;
        }
    }

    // Above we take care of all of the parsing for each command and have a map of integers to commands 
    // as well as if any commands have input or output files, now we will create pipes in the parent and 
    // continually loop and fork new children for each new command and check if that command is valid

    // Create an array of pipes that can be accessed by the children
    int num_pipes = num_commands-1;
    vector<int> pipe_fds(num_pipes*2);
    for(int k=0; k < num_pipes; k++){
        int returnVal = pipe(&pipe_fds[k*2]);
        if(returnVal < 0){
            std::cerr << "Pipe Error" << endl;
        }
    }

    // TESTING CODE FOR COMMAND PARSER
    // for(int i=0; i < num_commands; i++){
    //     cout << "Command #" << i << ":" << endl;
    //     for(int j=0; j < count_length_vec[i]; j++){
    //         cout << commands[i][j] << " ";
    //     }
    //     cout << endl;
    //     cout << "Length: " << count_length_vec[i] << " | Input Filename: " << input_files[i] << " | Output Filename: " << output_files[i] << endl; 
    // }
    // cout << "Pipes: " << num_pipes << " | Commands: " << num_commands << endl;

    // Spin up the appropriate number of child processes
    // Pipes created and closed inside parent shell (created before for // closed after fork and before wait)
    // Pipes get attached to appropriate input/output inside the child process
    for(int k = 0; k < num_commands; k++){
        //count_length is coun_length[k]t
        commands[k][count_length_vec[k]] = NULL;
    
        if(count_length_vec[k] == 0){
            // if it is a malformed command we do not want to execute it so just continue forward
            std::cerr << "invalid command\n";
            continue;
        }
        // if special keyword to exit the custom terminal
        if (command == "exit") { 
            exit(0); 
        }

        // FORKING TO CREATE A CHILD PROCESS
        int pid = fork();
        if(pid == 0){
            // INSIDE A SINGULAR CHILD PROCESS

            // Attach pipes to the appropriate input/output stuff here since every child gets a copy of the parent each child has a copy of each pipe we made
            // If we do not have any pipes then just skip pipe setup and execute the command 
            if(num_pipes==0){ }

            // the pipes have already been created think of this as connecting the N children to the N-1 pipes
            else if(num_pipes > 0){
                // If there is a pipe the first command should close the read end of the pipe and move the write end to stdout
                
                // pipe_fds[i*2] is the read end of the ith pipe
                // pipe_fds[i*2 + 1] is the write end of the ith pipe 
                if(k==0){
                    // connect write end to stdout
                    dup2(pipe_fds[(k*2)+1], 1);
                    close(pipe_fds[(k*2)+1]);
                    // producer should close read end and attach write to 1
                    close(pipe_fds[k*2]);
                }
                
                // If we are at the last command it should close to write end of the pipe to its left and attach the read end to stdin
                else if(k==num_pipes){
                    // consumer should close the write end of the file
                    close(pipe_fds[((k-1)*2) + 1]);
                    // connect its read end to stdin
                    dup2(pipe_fds[(k-1)*2], 0);
                    close(pipe_fds[(k-1)*2]);
                }

                // test/example_out.sh | test/example_sed2.sh | test/example_sed.sh
                // If we are at an intermediary command it should connect the read in of the previous pipe to stdin and write end to next pipe
                // think of an intermediate pipe as both a producer and a consumer
                else if(k > 0 && k < num_pipes){
                    dup2(pipe_fds[(k-1)*2], 0);
                    dup2(pipe_fds[(k*2) + 1], 1);
                }
            }
            // close all the pipes in the child on exec
            for(int i=0; i < num_pipes*2; i++){
                fcntl(pipe_fds[i], F_SETFD, FD_CLOEXEC);
            }
            // FILE REDIRECTION INSIDE THE CHILD
            if (input_files[k] != ""){
                close(0);
                int fd_in = open(input_files[k].c_str(), O_RDWR);
                if (fd_in < 0){
                    std::cerr << "invalid command";
                    continue;
                }
            }
            if (output_files[k] != ""){
                // this is saying that we are opening a file for read/write. Creating it if it doesnt exist and truncating it to 0 if it does
                // the last two arguments are also giving the permissions of the file
                // documentation found on this man page: https://pubs.opengroup.org/onlinepubs/9699919799.2013edition/functions/open.html
                close(1);
                int fd_out = open(output_files[k].c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (fd_out < 0){
                    std::cerr << "invalid command";
                    continue;
                }      
            }
            // EXECUTE THE COMMANDS THAT ARE PASSED IN
            int retVal = execv(commands[k][0], (char**)commands[k]);
            if (retVal < 0){
                std::cerr << "Exit Status 255 (Command not found). Error preventing running the executable.\n";
            }
        }

        // CATCH ANY ERRORS WHEN FORKING
        else if(pid < 0){
            std::cerr << "Fork Error.\n";
            continue;
        }
    }

    for(int i=0; i < num_pipes*2; i++){
        close(pipe_fds[i]);
    }
    pipe_fds.clear();

    // PARENT PROCESS LOOP WAITING FOR K CHILDREN TO FINISH
    // have a separate loop for the parent because we want only one single parent with many spun up children
    for(int k=0; k < num_commands; k++){
        // Before we hit the wait statement we will want to close the pipes b/c children have already been forked
        wait(&status);
        if (WIFEXITED(status)){
            cout << commands[k][0] << " exit status: " << WEXITSTATUS(status) << endl;
        }
    }

    // FREE ALL DYNAMICALLY ALLOCATED MEMORY
    delete[](argv);
}

// DRIVER CODE THAT CALLS OUR SHELL FOR EACH COMMAND
int main(void) {
    std::string command;
    std::cout << "> ";
    while (std::getline(std::cin, command)) {
        parse_and_run_command(command);
        std::cout << "> ";
    }
    return 0;
}