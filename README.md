# Posix_threads_and_sockets
POSIX Compliant C source code to control AAV with sockets

main program have 4 arguments:
  1. orders device TCP port
  2. first aav UDP port
  3. AAV IP address ("XXX.XXX.XXX.XXX")
  4. Timeout in seconds
  
  
orders program have 2 arguments:
  1. main device TCP port
  2. main device IP address ("XXX.XXX.XXX.XXX")
  
  
to simulate AAV in one device, each one will run in a thread and will send and receive data from different UDP ports
this program have 2 arguments:

  1. main device UDP port
  2. main device IP address ("XXX.XXX.XXX.XXX")
  
  
  
From orders device you can send:
  0: AAV will collect and send data via UDP to main and TCP to orders device
  1: AAV will land
  2: AAV will land and the program will finish (and also the main program)
