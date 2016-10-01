#include "FtpMessages.h"

const char *FtpMessage[] =
    {
        "ps2ftpd ready.",                   // FTPMSG_SERVER_READY
        "Goodbye.",                         // FTPMSG_GOODBYE
        "User logged in.",                  // FTPMSG_USER_LOGGED_IN
        "Password required for user.",      // FTPMSG_PASSWORD_REQUIRED_FOR_USER
        "Login incorrect.",                 // FTPMSG_LOGIN_INCORRECT
        "Login with USER first.",           // FTPMSG_LOGIN_WITH_USER_FIRST
        "Already authenticated.",           // FTPMSG_ALREADY_AUTHENTICATED
        "Entering passive mode",            // FTPMSG_ENTERING_PASSIVE_MODE
        "Could not enter passive mode.",    // FTPMSG_COULD_NOT_ENTER_PASSIVE_MODE
        "Command successful.",              // FTPMSG_COMMAND_SUCCESSFUL
        "Command failed.",                  // FTPMSG_COMMAND_FAILED
        "Illegal command.",                 // FTPMSG_ILLEGAL_COMMAND
        "Unable to open directory.",        // FTPMSG_UNABLE_TO_OPEN_DIRECTORY
        "File not found.",                  // FTPMSG_FILE_NOT_FOUND
        "Could not create file.",           // FTPMSG_COULD_NOT_CREATE_FILE
        "Could not get filesize.",          // FTPMSG_COULD_NOT_GET_FILESIZE
        "Requires parameters.",             // FTPMSG_REQUIRES_PARAMETERS
        "Not supported.",                   // FTPMSG_NOT_SUPPORTED
        "Not understood.",                  // FTPMSG_NOT_UNDERSTOOD
        "Opening ASCII connection.",        // FTPMSG_OPENING_ASCII_CONNECTION
        "Opening BINARY connection.",       // FTPMSG_OPENING_BINARY_CONNECTION
        "Unable to build data connection",  // FTPMSG_UNABLE_TO_BUILD_DATA_CONNECTION
        "Transfer failed.",                 // FTPMSG_TRANSFER_FAILED
        "Local write failed.",              // FTPMSG_LOCAL_WRITE_FAILED
        "Failed reading data.",             // FTPMSG_FAILED_READING_DATA
        "Premature client disconnect.",     // FTPMSG_PREMATURE_CLIENT_DISCONNECT
        "Invalid restart marker.",          // FTPMSG_INVALID_RESTART_MARKER
        "Restart marker set.",              // FTPMSG_RESTART_MARKER_SET
};
