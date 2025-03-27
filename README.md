Simple Remote Command Executor 🚀💻
Welcome to the Simple Remote Command Executor project! This project is a remote administration application that allows you to:

Download Files or Folders 📎➡️💾
The download feature enables you to retrieve files or folders from a remote machine easily.

Execute Remote Commands ⌨️🖥️
Run CMD commands like cd, cls, and other commands remotely, allowing you to control the target machine in real-time.

Application Overview 📄
This project consists of two main applications:

Server (server.c)
The server waits for connections from the client, receives commands, and sends execution results or file/folder data to the client.

It also displays the currently active working path.

Client (client.c)
The client sends commands to the server, receives and displays execution results, and initiates the file/folder download process.

Key Features 🌟
✅ File/Folder Download

Using the command download <path>, the server will check the type of the path (file or folder) and initiate the data transfer accordingly.

✅ Remote Command Execution

You can execute any CMD command such as dir, ipconfig, or other custom commands.

The results are sent back to the client and displayed instantly.

✅ Stable Connection

Includes retry and timeout mechanisms to maintain a stable connection between the server and client, even in case of interruptions.

✅ Secure Data Transmission with Checksum 🔒✅

Ensures data integrity through checksum validation during each data transfer.

System Requirements ⚙️
Platform
This application is written using the Windows API (Winsock) and can only be run on Windows.

Compiler
Use Visual Studio or any other C/C++ compiler that supports Windows.

Dependencies
Winsock Library (ws2_32.lib)

Shlwapi Library (Shlwapi.lib) – required for the client application.

Compilation Guide 🛠️
Using Visual Studio
Open Visual Studio and create a new Console Application project for each file (server.c and client.c).

Add Source Files:

Add server.c for the server application.

Add client.c for the client application.

Configure Libraries:

Add ws2_32.lib (and Shlwapi.lib for the client) under Linker → Input.

Build each project.

Using Command Line (MSVC)
Open the Developer Command Prompt and use the following commands:

Server
sh
Copy
Edit
cl /EHsc server.c ws2_32.lib
Client
sh
Copy
Edit
cl /EHsc client.c ws2_32.lib Shlwapi.lib
Usage Instructions 🚀
Running the Application
Start the Server:
Run server.exe.

The server will start listening on port 8080 and display the current working directory.

Wait for a client to connect. 👀

Start the Client:
Run client.exe.

The client will attempt to connect to the server using IP 192.168.31.94 and port 8080 (default).

Once connected, you can send commands to the server. 👍

Example Commands
Download File/Folder:
sh
Copy
Edit
download <path>
Example:

sh
Copy
Edit
download C:\Users\Documents\file.txt
The server will check the path type and begin transferring the file/folder.

Run CMD Commands:
Type commands like dir, ipconfig, or any other command to execute on the server machine.

The output will be sent back to the client and displayed on the screen. 🔄

Change Directory:
sh
Copy
Edit
cd <path>
Changes the server’s working directory.

Exit the Application:
Type exit in the client to terminate the remote session.

Project Structure 💃
bash
Copy
Edit
SimpleRemoteCommandExecutor/
├── server.c              # Server application (receives commands & sends data)
├── client.c              # Client application (sends commands & receives data)
├── TCPBlockTransRecv.h   # Header for data transmission with checksum
├── SendFile.h            # Header for file transfer functions
├── SendDir.h             # Header for folder transfer functions
└── README.md             # Project documentation
Important Notes ⚠️
Network Connection
Ensure that both server and client are on the same network or properly configured for remote access.

Security Considerations 🔓
This application does not encrypt data. Be cautious when using it on untrusted networks.

Debugging
If any errors occur, messages will be displayed in the console.

Check the logs for more details.
