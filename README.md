Instructions on how to run and test the program:
1.Run the database initialization script: bash initData.sh
2.Create executables: make all
3.Start all the servers at once in background: bash startAllServers.sh
Or run each server individualy with an configuration file: ./server config/exampleConf.txt
4.Run the resolver with, no parameters, in which case you'll be prompted to provide a domain name, 1 parameter, 
which will consist of the domain name, or 2 parameters, the second one being the name of a file where the response will be stored: ./resolver \[domainName\] \[file\]