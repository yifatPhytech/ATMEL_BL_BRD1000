#ifndef CLI_H_
#define CLI_H_

#ifdef Cli
void cliTx(char c);
void cliTask();
#else
#define cliTx(c)
#define cliTask()
#endif

#endif /* CLI_H_ */