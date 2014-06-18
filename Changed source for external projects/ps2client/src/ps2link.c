//----------------------------------------------------------------------------
//File name:    ps2link.c                       Revision date:  2006.02.17
//----------------------------------------------------------------------------
 #include <stdio.h>
 #include <fcntl.h>
 #include <string.h>
 #include <dirent.h>
 #include <unistd.h>
 #include <time.h>
 #include <sys/stat.h>
 #include <pthread.h>
#ifndef _WIN32
 #include <netinet/in.h>
#else
 #include <windows.h>
 #define sleep(x) Sleep(x * 1000)
#endif

 #include "network.h"
 #include "ps2link.h"
 #include "utility.h"

 int console_socket = -1;
 int request_socket = -1;
 int command_socket = -1;

 pthread_t console_thread_id;
 pthread_t request_thread_id;

 int ps2link_counter = 0;

 DIR *ps2link_dd[10] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

 char old_pathname[256]; //dlanor: Used in open/opendir. Needed for Rename Ioctl.
 int  old_fd_dd;         //dlanor: fd/dd associated with old_name.
 int  old_req;           //dlanor: Previous request ID. For Rename Ioctl validity.
//----------------------------------------------------------------------------
 ///////////////////////
 // PS2LINK FUNCTIONS //
 ///////////////////////
//----------------------------------------------------------------------------
 int ps2link_connect(char *hostname) {

  // Connect to the console port.
  console_socket = network_listen(0x4712, SOCK_DGRAM);

  // Create the console thread.
  if (console_socket > 0) { pthread_create(&console_thread_id, NULL, ps2link_thread_console, (void *)&console_thread_id); }

  // Connect to the request port.
  request_socket = network_connect(hostname, 0x4711, SOCK_STREAM);

  // Create the request thread.
  if (request_socket > 0) { pthread_create(&request_thread_id, NULL, ps2link_thread_request, (void *)&request_thread_id); }

  // Connect to the command port.
  command_socket = network_connect(hostname, 0x4712, SOCK_DGRAM);

  // Delay for a moment to let ps2link finish setup.
#ifdef _WIN32
  Sleep(1);
#else
  sleep(1);
#endif

  // End function.
  return 0;
 }
//----------------------------------------------------------------------------
 int ps2link_mainloop(int timeout) {

  // Disconnect from the command port.
  if (network_disconnect(command_socket) < 0) { return -1; }

  // If no timeout was given, timeout immediately.
  if (timeout == 0) { return 0; }

  // If timeout was never, loop forever.
  if (timeout < 0) { for (;;) { sleep(600); } }

  // Increment the timeout counter until timeout is reached.
  while (ps2link_counter++ < timeout) { sleep(1); };

  // End function.
  return 0;

 }
//----------------------------------------------------------------------------
 int ps2link_disconnect(void) {

  // Disconnect from the command port.
  if (network_disconnect(command_socket) < 0) { return -1; }

  // Disconnect from the request port.
  if (network_disconnect(request_socket) < 0) { return -1; }

  // Disconnect from console port.
  if (network_disconnect(console_socket) < 0) { return -1; }

  // End function.
  return 0;

 }
//----------------------------------------------------------------------------
 ///////////////////////////////
 // PS2LINK COMMAND FUNCTIONS //
 ///////////////////////////////
//----------------------------------------------------------------------------
 int ps2link_command_reset(void) {
  struct { unsigned int number; unsigned short length; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_RESET);
  command.length = htons(sizeof(command));

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_execiop(int argc, char **argv) {
  struct { unsigned int number; unsigned short length; int argc; char argv[256]; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_EXECIOP);
  command.length = htons(sizeof(command));
  command.argc   = htonl(argc);
  fix_argv(command.argv, argv);

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_execee(int argc, char **argv) {
  struct { unsigned int number; unsigned short length; int argc; char argv[256]; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_EXECEE);
  command.length = htons(sizeof(command));
  command.argc   = htonl(argc);
  fix_argv(command.argv, argv);

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_poweroff(void) {
  struct { unsigned int number; unsigned short length; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_POWEROFF);
  command.length = htons(sizeof(command));

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_scrdump(void) {
  struct { unsigned int number; unsigned short length; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_SCRDUMP);
  command.length = htons(sizeof(command));

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_netdump(void) {
  struct { unsigned int number; unsigned short length; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_NETDUMP);
  command.length = htons(sizeof(command));

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));

 }
//----------------------------------------------------------------------------
 int ps2link_command_dumpmem(unsigned int offset, unsigned int size, char *pathname) {
  struct { unsigned int number; unsigned short length; unsigned int offset; unsigned int size; char pathname[256]; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_DUMPMEM);
  command.length = htons(sizeof(command));
  command.offset = htonl(offset);
  command.size   = htonl(size);
  if (pathname) { strncpy(command.pathname, pathname, 256); }

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_startvu(int vu) {
  struct { unsigned int number; unsigned short length; int vu; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_STARTVU);
  command.length = htons(sizeof(command));
  command.vu     = htonl(vu);

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_stopvu(int vu) {
  struct { unsigned int number; unsigned short length; int vu; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_STOPVU);
  command.length = htons(sizeof(command));
  command.vu     = htonl(vu);

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_dumpreg(int type, char *pathname) {
  struct { unsigned int number; unsigned short length; int type; char pathname[256]; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_DUMPREG);
  command.length = htons(sizeof(command));
  command.type   = htonl(type);
  if (pathname) { strncpy(command.pathname, pathname, 256); }

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_gsexec(unsigned short size, char *pathname) {
  struct { unsigned int number; unsigned short length; unsigned short size; char pathname[256]; } PACKED command;

  // Build the command packet..
  command.number = htonl(PS2LINK_COMMAND_GSEXEC);
  command.length = htons(sizeof(command));
  command.size   = htonl(size);
  if (pathname) { strncpy(command.pathname, pathname, 256); }

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_writemem(unsigned int offset, unsigned int size, char *pathname) {
  struct { unsigned int number; unsigned short length; unsigned int offset; unsigned int size; char pathname[256]; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_WRITEMEM);
  command.length = htons(sizeof(command));
  command.offset = htonl(offset);
  command.size   = htonl(size);
  if (pathname) { strncpy(command.pathname, pathname, 256); }

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));
 }
//----------------------------------------------------------------------------
 int ps2link_command_iopexcep(void) {
  struct { unsigned int number; unsigned short length; } PACKED command;

  // Build the command packet.
  command.number = htonl(PS2LINK_COMMAND_IOPEXCEP);
  command.length = htons(sizeof(command));

  // Send the command packet.
  return network_send(command_socket, &command, sizeof(command));

  // End function.
  return 0;
 }
//----------------------------------------------------------------------------
 ///////////////////////////////
 // PS2LINK REQUEST FUNCTIONS //
 ///////////////////////////////
//----------------------------------------------------------------------------
 int ps2link_request_open(void *packet) {
  struct { unsigned int number; unsigned short length; int flags; char pathname[256]; } PACKED *request = packet;
  int result = -1;

  // Fix the arguments.
  fix_pathname(request->pathname);
  request->flags = fix_flags(ntohl(request->flags));

  // Perform the request.
  result = open(request->pathname, request->flags, 0644);

  strcpy(old_pathname, request->pathname);  //dlanor: needed for Ioctl Rename
  old_fd_dd = result;  //dlanor: Allows Ioctl Rename to check file validity

  // Send the response.
  return ps2link_response_generic(result, PS2LINK_RESPONSE_OPEN);
 }
//----------------------------------------------------------------------------
 int ps2link_request_close(void *packet) {
  struct { unsigned int number; unsigned short length; int fd; } PACKED *request = packet;
  int result = -1;

  // Perform the request.
  result = close(ntohl(request->fd));

  // Send the response.
  return ps2link_response_generic(result, PS2LINK_RESPONSE_CLOSE);
 }
//----------------------------------------------------------------------------
 int ps2link_request_read(void *packet) {
  struct { unsigned int number; unsigned short length; int fd; int size; } PACKED *request = packet;
  int result = -1, size = -1; char buffer[65536], *bigbuffer;

  // If a big read is requested...
  if (ntohl(request->size) > sizeof(buffer)) {

   // Allocate the bigbuffer.
   bigbuffer = malloc(ntohl(request->size));

   // Perform the request.
   result = size = read(ntohl(request->fd), bigbuffer, ntohl(request->size));

   // Send the response.
   ps2link_response_read(result, size);

   // Send the response data.
   network_send(request_socket, bigbuffer, size);

   // Free the bigbuffer.
   free(bigbuffer);

  // Else, a normal read is requested...
  } else {

   // Perform the request.
   result = size = read(ntohl(request->fd), buffer, ntohl(request->size));

   // Send the response.
   ps2link_response_read(result, size);

   // Send the response data.
   network_send(request_socket, buffer, size);
  }

  // End function.
  return 0;
 }
//----------------------------------------------------------------------------
 int ps2link_request_write(void *packet) {
  struct { unsigned int number; unsigned short length; int fd; int size; } PACKED *request = packet;
  int result = -1; char buffer[65536], *bigbuffer;

  // If a big write is requested...
  if (ntohl(request->size) > sizeof(buffer)) {

   // Allocate the bigbuffer.
   bigbuffer = malloc(ntohl(request->size));

   // Read the request data.
   network_receive_all(request_socket, bigbuffer, ntohl(request->size));

   // Perform the request.
   result = write(ntohl(request->fd), bigbuffer, ntohl(request->size));

   // Send the response.
   ps2link_response_generic(result, PS2LINK_RESPONSE_WRITE);

   // Free the bigbuffer.
   free(bigbuffer);

  // Else, a normal write is requested...
  } else {

   // Read the request data.
   network_receive_all(request_socket, buffer, ntohl(request->size));

   // Perform the request.
   result = write(ntohl(request->fd), buffer, ntohl(request->size));

   // Send the response.
   ps2link_response_generic(result, PS2LINK_RESPONSE_WRITE);

  }

  // End function.
  return 0;
 }
//----------------------------------------------------------------------------
 int ps2link_request_lseek(void *packet) {
  struct { unsigned int number; unsigned short length; int fd, offset, whence; } PACKED *request = packet;
  int result = -1;

  // Perform the request.
  result = lseek(ntohl(request->fd), ntohl(request->offset), ntohl(request->whence));

  // Send the response.
  return ps2link_response_generic(result, PS2LINK_RESPONSE_LSEEK);
 }
//----------------------------------------------------------------------------
 int ps2link_request_opendir(void *packet) { int loop0 = 0;
  struct { unsigned int command; unsigned short length; int flags; char pathname[256]; } PACKED *request = packet;
  int result = -1;

  // Fix the arguments.
  fix_pathname(request->pathname);

  // Allocate an available directory descriptor.
  for (loop0=0; loop0<10; loop0++) { if (ps2link_dd[loop0] == NULL) { result = loop0; break; } }

  // Perform the request.
  if (result != -1) { ps2link_dd[result] = opendir(request->pathname); }

  strcpy(old_pathname, request->pathname);  //dlanor: needed for Ioctl Rename
  old_fd_dd = result;  //dlanor: Allows Ioctl Rename to check folder validity

  // Send the response.
  return ps2link_response_generic(result, PS2LINK_RESPONSE_OPENDIR);
 }
//----------------------------------------------------------------------------
 int ps2link_request_closedir(void *packet) {
  struct { unsigned int number; unsigned short length; int dd; } PACKED *request = packet;
  int result = -1;

  // Perform the request.
  result = closedir(ps2link_dd[ntohl(request->dd)]);

  // Free the directory descriptor.
  ps2link_dd[ntohl(request->dd)] = NULL;

  // Send the response.
  return ps2link_response_generic(result, PS2LINK_RESPONSE_CLOSEDIR);
 }
//----------------------------------------------------------------------------
 int ps2link_request_readdir(void *packet) {
  struct { unsigned int number; unsigned short length; int dd; } PACKED *request = packet;
  struct dirent *dirent; struct stat stats; struct tm loctime;
  unsigned int mode; unsigned char ctime[8]; unsigned char atime[8]; unsigned char mtime[8];

  // Perform the request.
  dirent = readdir(ps2link_dd[ntohl(request->dd)]);

  // If no more entries were found...
  if (dirent <= 0) {

   // Tell the user an entry wasn't found.
   return ps2link_response_readdir(0, 0, 0, 0, NULL, NULL, NULL, 0, NULL);

  }

  // Fetch the entry's statistics.
  stat(dirent->d_name, &stats);

  // Convert the mode.
  mode = (stats.st_mode & 0x07);
  if (S_ISDIR(stats.st_mode)) { mode |= 0x20; }
#ifndef _WIN32
  if (S_ISLNK(stats.st_mode)) { mode |= 0x08; }
#endif
  if (S_ISREG(stats.st_mode)) { mode |= 0x10; }

  // Convert the creation time.
  localtime_r(&(stats.st_ctime), &loctime);
  ctime[6] = (unsigned char)loctime.tm_year;
  ctime[5] = (unsigned char)loctime.tm_mon + 1;
  ctime[4] = (unsigned char)loctime.tm_mday;
  ctime[3] = (unsigned char)loctime.tm_hour;
  ctime[2] = (unsigned char)loctime.tm_min;
  ctime[1] = (unsigned char)loctime.tm_sec;

  // Convert the access time.
  localtime_r(&(stats.st_atime), &loctime);
  atime[6] = (unsigned char)loctime.tm_year;
  atime[5] = (unsigned char)loctime.tm_mon + 1;
  atime[4] = (unsigned char)loctime.tm_mday;
  atime[3] = (unsigned char)loctime.tm_hour;
  atime[2] = (unsigned char)loctime.tm_min;
  atime[1] = (unsigned char)loctime.tm_sec;

  // Convert the last modified time.
  localtime_r(&(stats.st_mtime), &loctime);
  mtime[6] = (unsigned char)loctime.tm_year;
  mtime[5] = (unsigned char)loctime.tm_mon + 1;
  mtime[4] = (unsigned char)loctime.tm_mday;
  mtime[3] = (unsigned char)loctime.tm_hour;
  mtime[2] = (unsigned char)loctime.tm_min;
  mtime[1] = (unsigned char)loctime.tm_sec;

  // Send the response.
  return ps2link_response_readdir(1, mode, 0, stats.st_size, ctime, atime, mtime, 0, dirent->d_name);
 }
//----------------------------------------------------------------------------
 int ps2link_request_remove(void *packet) {
  struct { unsigned int number; unsigned short length; char name[256]; } PACKED *request = packet;
  int result = -1;

  // Fix the arguments.
  fix_pathname(request->name);

  // Perform the request.
  result = remove(request->name);

  // Send the response.
  return ps2link_response_generic(result, PS2LINK_RESPONSE_REMOVE);
 }
//----------------------------------------------------------------------------
 int ps2link_request_mkdir(void *packet) {
  struct { unsigned int number; unsigned short length; int mode; char name[256]; } PACKED *request = packet;
  int result = -1;

  // Fix the arguments.
  fix_pathname(request->name);

  // Perform the request.
  // do we need to use mode in here: request->mode  ?
  //result = mkdir(request->name, 0);  //dlanor: Invalid arguments for my MinGW libs
  result = mkdir(request->name);  //dlanor: Valid argument for my MinGW libs

  // Send the response.
  return ps2link_response_generic(result, PS2LINK_RESPONSE_MKDIR);
 };
//----------------------------------------------------------------------------
 int ps2link_request_rmdir(void *packet) {
  struct { unsigned int number; unsigned short length; char name[256]; } PACKED *request = packet;
  int result = -1;

  // Fix the arguments.
  fix_pathname(request->name);

  // Perform the request.
  result = rmdir(request->name);

  // Send the response.
  return ps2link_response_generic(result, PS2LINK_RESPONSE_RMDIR);
 };
//----------------------------------------------------------------------------
 int ps2link_request_ioctl(void *packet) {  //dlanor: Added for Rename capability
  struct { unsigned int number; unsigned short length; int fd_dd; int request_code; char ioctl_data[256]; } PACKED *request = packet;
  int result = -1;
  int fd_dd = ntohl(request->fd_dd);
	int request_code = ntohl(request->request_code);
	char *ioctl_data = request->ioctl_data;

//dlanor: The statements below implement a 'Rename' function using Ioctl.
//dlanor: Correct PS2 usage of it will be to use fioOpen or fioDopen, to select
//dlanor: the file or folder to be renamed, then following this immediately
//dlanor: with a call to fioIoctl, with the function request argument set to
//dlanor: IOCTL_RENAME, and the data pointer pointing to the new name.
//dlanor: When used correctly this will result in the file/folder first being
//dlanor: closed, and then renamed. Error codes are returned for all errors that
//dlanor: may occur during this processing. At the PS2 end, an additional call
//dlanor: should be made to fioClose, so as to free the file/folder descriptor
//dlanor: used internally by IOMAN, but that call should not lead to any network
//dlanor: traffic with the PC, as the file/folder is already closed at that end.

  if (request_code == IOCTL_RENAME){
    if (old_fd_dd == fd_dd) {
      if (old_req == PS2LINK_REQUEST_OPEN) {
        result = close(old_fd_dd);
        if (result < 0) result = -4;  //Error: Error closing file in Ioctl Rename
      } else if (old_req == PS2LINK_REQUEST_OPENDIR) {
        result = closedir(ps2link_dd[old_fd_dd]);
        ps2link_dd[old_fd_dd] = NULL;
        if (result < 0) result = -5;  //Error: Error closing folder in Ioctl Rename
      } else result = -3;  //Error: Erroneous usage of Ioctl Rename function
      if(result >= 0){
      	char *new_pathname = ioctl_data;
        new_pathname[255] = '\0';    //Enforce string termination of new pathname
        fix_pathname(new_pathname);  //fix new pathname in same way as old
        result = rename(old_pathname, new_pathname);  //Try to rename
        if (result < 0) result = -6;  //Error: Correctly used Ioctl Rename failed
      }
    } else result = -2;  //Error: Invalid file/dir descriptor for Ioctl Rename function
  } else result = -1;  //Error: Invalid Ioctl function request code

  // Send the response.
  return ps2link_response_generic(result, PS2LINK_RESPONSE_IOCTL);
 };
//----------------------------------------------------------------------------
 ////////////////////////////////
 // PS2LINK RESPONSE FUNCTIONS //
 ////////////////////////////////
//----------------------------------------------------------------------------
 int ps2link_response_read(int result, int size) {
  struct { unsigned int number; unsigned short length; int result; int size; } PACKED response;

  // Build the response packet.
  response.number = htonl(PS2LINK_RESPONSE_READ);
  response.length = htons(sizeof(response));
  response.result = htonl(result);
  response.size   = htonl(size);

  // Send the response packet.
  return network_send(request_socket, &response, sizeof(response));
 }
//----------------------------------------------------------------------------
 int ps2link_response_readdir(int result, unsigned int mode, unsigned int attr, unsigned int size, unsigned char *ctime, unsigned char *atime, unsigned char *mtime, unsigned int hisize, char *name) {
  struct { unsigned int number; unsigned short length; int result; unsigned int mode; unsigned int attr; unsigned int size; unsigned char ctime[8]; unsigned char atime[8]; unsigned char mtime[8]; unsigned int hisize; char name[256]; } PACKED response;

  // Build the response packet.
  response.number = htonl(PS2LINK_RESPONSE_READDIR);
  response.length = htons(sizeof(response));
  response.result = htonl(result);
  response.mode   = htonl(mode);
  response.attr   = htonl(attr);
  response.size   = htonl(size);
  if (ctime) { memcpy(response.ctime, ctime, 8); }
  if (atime) { memcpy(response.atime, atime, 8); }
  if (mtime) { memcpy(response.mtime, mtime, 8); }
  response.hisize = htonl(hisize);
#ifdef _WIN32
  if (name) { sprintf(response.name, "%s", name); }
#else
  if (name) { snprintf(response.name, 256, "%s", name); }
#endif

  // Send the response packet.
  return network_send(request_socket, &response, sizeof(response));
 }
//----------------------------------------------------------------------------
 int ps2link_response_generic(int result, int response_ID) { //dlanor: for standard responses
  struct { unsigned int number; unsigned short length; int result; } PACKED response;

  // Build the response packet.
  response.number = htonl(response_ID);
  response.length = htons(sizeof(response));
  response.result = htonl(result);

  // Send the response packet.
  return network_send(request_socket, &response, sizeof(response));
 }
//----------------------------------------------------------------------------
 //////////////////////////////
 // PS2LINK THREAD FUNCTIONS //
 //////////////////////////////
//----------------------------------------------------------------------------

 void *ps2link_thread_console(void *thread_id) {
  char buffer[1024];

  // If the socket isn't open, this thread isn't needed.
  if (console_socket < 0) { pthread_exit(thread_id); }

  // Loop forever...
  for (;;) {

   // Wait for network activity.
   network_wait(console_socket, -1);

   // Receive the console buffer.
   network_receive(console_socket, buffer, sizeof(buffer));

   // Print out the console buffer.
   printf("%s", buffer);

   // Clear the console buffer.
   memset(buffer, 0, sizeof(buffer));

   // Reset the timeout counter.
   ps2link_counter = 0;
  }
 }
//----------------------------------------------------------------------------
 void *ps2link_thread_request(void *thread_id) {
  struct { unsigned int number; unsigned short length; char buffer[512]; } PACKED packet;
	static int	new_req;

  // If the socket isn't open, this thread isn't needed.
  if (request_socket < 0) { pthread_exit(thread_id); }

  // Loop forever...  
  for (;;) {

   // Wait for network activity.
   network_wait(request_socket, -1);

   // Read in the request packet header.
   network_receive_all(request_socket, &packet, 6);

   // Read in the rest of the packet.
   network_receive_all(request_socket, packet.buffer, ntohs(packet.length) - 6);

   old_req = new_req;  //dlanor: allows request functions to test previous request code
   new_req = ntohl(packet.number);
   // Perform the requested action.
   if (new_req == PS2LINK_REQUEST_OPEN)     { ps2link_request_open(&packet);     } else
   if (new_req == PS2LINK_REQUEST_CLOSE)    { ps2link_request_close(&packet);    } else
   if (new_req == PS2LINK_REQUEST_READ)     { ps2link_request_read(&packet);     } else
   if (new_req == PS2LINK_REQUEST_WRITE)    { ps2link_request_write(&packet);    } else
   if (new_req == PS2LINK_REQUEST_LSEEK)    { ps2link_request_lseek(&packet);    } else
   if (new_req == PS2LINK_REQUEST_OPENDIR)  { ps2link_request_opendir(&packet);  } else
   if (new_req == PS2LINK_REQUEST_CLOSEDIR) { ps2link_request_closedir(&packet); } else
   if (new_req == PS2LINK_REQUEST_READDIR)  { ps2link_request_readdir(&packet);  } else
   if (new_req == PS2LINK_REQUEST_REMOVE)   { ps2link_request_remove(&packet);   } else
   if (new_req == PS2LINK_REQUEST_MKDIR)    { ps2link_request_mkdir(&packet);    } else
   if (new_req == PS2LINK_REQUEST_RMDIR)    { ps2link_request_rmdir(&packet);    } else
   if (new_req == PS2LINK_REQUEST_IOCTL)    { ps2link_request_ioctl(&packet);    }

   // Reset the timeout counter.
   ps2link_counter = 0;
  }
 }
//----------------------------------------------------------------------------
//End of file:  ps2link.c
//----------------------------------------------------------------------------
