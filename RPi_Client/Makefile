all:CTL_rpio.out

OBJS	= pwm.o CTL_rpio.o

CTL_rpio.out:$(OBJS)
	gcc -o CTL_rpio.out -l rt -l wiringPi $(OBJS)

CTL_rpio.o:CTL_rpio.c
	gcc -c -o CTL_rpio.o CTL_rpio.c

pwm.o:pwm.c
	gcc -Wall -g -O2 -c -o pwm.o pwm.c

pwm.o:pwm.h
CTL_rpio.o:pinAssign.h

.PHONY:clean
.PHONY:all
clean:
	rm -rf CTL_rpio.out $(OBJS)

