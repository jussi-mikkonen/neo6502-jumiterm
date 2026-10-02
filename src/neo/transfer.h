/* src/neo/transfer.h - the Download and Upload menu entries. */
#ifndef TRANSFER_H
#define TRANSFER_H

/* Each asks for the protocol (and a file), runs the transfer and shows the
   result until a key is pressed. The caller redraws the status bar. */
void transfer_download(void);
void transfer_upload(void);

#endif
