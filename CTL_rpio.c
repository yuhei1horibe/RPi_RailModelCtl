#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wiringPi.h>
#include "pinAssign.h"
#include <signal.h>
#include "pwm.h"

//制御用変数
//PWM出力値
int	pwmCnt1	= 0;
int	pwmCnt2	= 0;

//ノッチ入力
int	Notch1	= 0;
int	Notch2	= 0;

//出力先
int	gpio_ch1	= 18;	//デフォルトは18ピン
int	gpio_ch2	= 24;	//コントローラ2は24ピン

//コントローラの設定
int	ctl_sel	= 1;	//どっちのコントローラへの指令か

//キャリア分散タイミング決定用
int	curRange;
int	ChgTim	= 0;

//18ピンをPWM出力に設定
void InitPWM()
{
	pinMode(GPIO18, PWM_OUTPUT);
	pwmSetMode(PWM_MODE_MS);
	pwmSetClock(25);
	pwmSetRange(1024);

	//今のレンジを記憶
	curRange	= 1024;

	srand(100);
}

//緊急停止
void stopPWM(int signu)
{
	int	i;

	for(i = 1; i >= 0; i -= 0.1){
		pwmWrite(GPIO18, pwmCnt1 * i);
		delay(10);
	}
	delay(500);

	digitalWrite(GPIO22, 0);

	printf("Exiting program...\n");
	exit(1);
}

//タイマハンドラ
void tim1ms_handler(int signum)
{
	int	accel	= 0;
	int	decel	= 0;

	//キャリアは毎周期変更
	//pwmSetRange(curRange = 1024 + rand() % 1024);	//350Hzから750Hzまで分散

	//指令値を変更
	add_channel_pulse(0, gpio_ch1, 10, 940 * pwmCnt1 / 4096);
	add_channel_pulse(1, gpio_ch2, 10, 940 * pwmCnt2 / 4096);

	//指令値の変更は10ms周期
	ChgTim	= (ChgTim + 1) % 5;

	//最高値が変わるので、値を更新
	//pwmWrite(GPIO18, curRange * pwmCnt1 / 4096);

	if(ChgTim != 0)
		return ;

	//コントローラ1と2を並行して更新するため2個同時に計算
	if((Notch1 <= 5) && (Notch1 > -6)){
		pwmCnt1	= (pwmCnt1 + Notch1 * 1 > 4096) ? 4096 : (pwmCnt1 + Notch1 * 1 < 0 ? 0 : pwmCnt1 + Notch1 * 1);
	}

	if((Notch2 <= 5) && (Notch2 > -6)){
		pwmCnt2	= (pwmCnt2 + Notch2 * 1 > 4096) ? 4096 : (pwmCnt2 + Notch2 * 1 < 0 ? 0 :  pwmCnt2 + Notch2 * 1);
	}

	return ;
}

int main(int argc, char *argv[])
{
	int			i;

	char			buff[512];
	int			tmp_input;

	//PWMはRPIOを使用する
	setup(PULSE_WIDTH_INCREMENT_GRANULARITY_US_DEFAULT, DELAY_VIA_PWM);
	init_channel(0, 10000);
	init_channel(1, 10000);

	//signal関係
	struct sigaction	sa_tim, def_tim_hdr;

	//タイマー
	timer_t			tid;
	struct itimerspec	itval;

	//10msごとのタイマハンドラ（PWM制御用）を定義
	memset(&sa_tim, 0, sizeof(struct sigaction));
	memset(&def_tim_hdr, 0, sizeof(struct sigaction));
	sa_tim.sa_handler	= tim1ms_handler;
	sa_tim.sa_flags		|= SA_RESTART;

	if(sigaction(SIGALRM, &sa_tim, &def_tim_hdr) != 0){
		printf("Registration of timer handler failed!\n");
		return -1;
	}

	//タイマ割り込みの設定
	itval.it_value.tv_sec	= 1;	//最初の割り込み
	itval.it_value.tv_nsec	= 0;

	itval.it_interval.tv_sec	= 0;
	itval.it_interval.tv_nsec	= 2000000;	//2ms周期

	//タイマ作成
	if(timer_create(CLOCK_REALTIME, NULL, &tid) != 0){
		printf("Timer creation failed.\n");
		exit(1);
	}

	//割り込み周期設定
	if(timer_settime(tid, 0, &itval, NULL) != 0){
		printf("Set timer error.\n");
		exit(1);
	}

	//GPIOのイニシャライズ
	if(wiringPiSetup() == -1){
		printf("Initialization of wiringPi failed.\n");
		return -1;
	}

	//出力設定
	pinMode(GPIO22, OUTPUT);	//MC
	pinMode(GPIO18, OUTPUT);	//pwm1
	pinMode(GPIO23, OUTPUT);	//pwm2
	pinMode(GPIO24, OUTPUT);	//pwm3
	pinMode(GPIO25, OUTPUT);	//pwm4

	//2ポート同時にターンオンしてしまうバグ対策。一度0を書き込む
	digitalWrite(GPIO22, 0);
	digitalWrite(GPIO18, 1);
	digitalWrite(GPIO23, 1);
	digitalWrite(GPIO24, 1);
	digitalWrite(GPIO25, 1);

	delay(500);

	digitalWrite(GPIO18, 0);
	digitalWrite(GPIO23, 0);
	digitalWrite(GPIO24, 0);
	digitalWrite(GPIO25, 0);

	delay(500);

	//22ピン=接触器を投入
	digitalWrite(GPIO22, 1);
	delay(1000);

	//18ピンをPWM出力モードに設定
	//InitPWM();

	//コンソールからの入力でノッチを決定
	while(1){
		printf("Waiting for the input...\n");
		gets(buff);

		if(strcmp(buff, "exit") == 0){
			printf("Exitting program...\n");
			break;
		}

		else if(strcmp(buff, "F") == 0){
			if((ctl_sel == 1) && (pwmCnt1 == 0)){
				gpio_ch1	= 18;
				printf("ch:%d\n", gpio_ch1);
			}

			else if((ctl_sel == 2) && (pwmCnt2 == 0)){
				gpio_ch2	= 24;
				printf("ch:%d\n", gpio_ch2);
			}
		}

		else if(strcmp(buff, "R") == 0){
			if((ctl_sel == 1) && (pwmCnt1 == 0)){
				gpio_ch1	= 23;
				printf("ch:%d\n", gpio_ch1);
			}

			else if((ctl_sel == 2) && (pwmCnt2 == 0)){
				gpio_ch2	= 25;
				printf("ch:%d\n", gpio_ch2);
			}
		}

		else if(strcmp(buff, "CTL1") == 0)
			ctl_sel	= 1;

		else if(strcmp(buff, "CTL2") == 0)
			ctl_sel	= 2;

		//ノッチを取得
		tmp_input	= atoi(buff);

		if(tmp_input < -5 || tmp_input > 5)
			tmp_input	= 0;

		//ノッチを更新
		if(ctl_sel == 1)
			Notch1	= tmp_input;

		else if(ctl_sel == 2)
			Notch2	= tmp_input;
	}

	delay(1000);

	//ゲートストップ
	//pwmWrite(GPIO18, 0);

	//RPIOを開放
	shutdown();

	//接触器をオープン
	digitalWrite(GPIO22, 0);

	//すべてのPWM出力をオフ
	digitalWrite(GPIO22, 0);
	digitalWrite(GPIO18, 0);
	digitalWrite(GPIO23, 0);
	digitalWrite(GPIO24, 0);
	digitalWrite(GPIO25, 0);

	//タイマを解放
	timer_delete(tid);

	//シグナルハンドラ解除
	sigaction(SIGALRM, &def_tim_hdr, NULL);

	return 0;
}
