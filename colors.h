#ifndef COLORS_H
#define COLORS_H

#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YLW "\e[0;33m"
#define CRT "\e[0m"
#define BLE "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define BBLE "\e[1;94m"

#define SUCCESS "\n" GRN "#>" CRT
#define ERROR   "\n" RED "#>" CRT
#define INFO    "\n" YLW "#>" CRT
#define CMD     "\n" BBLE "#>" CRT
#define INPUT   BLE "#>" CRT

#define CLR     "\033[2J\033[H"

#endif
