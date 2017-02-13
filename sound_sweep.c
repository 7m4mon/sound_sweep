/*
2016年12月12日
息子のために音が変わるオモチャを作る
Memory usage:   ROM=49%      RAM=50% - 72%
*/
#include <12f675.h>					//アナログポートを使う。
#DEVICE ADC=8

#define ADC_WAIT_US		20			//ADCの変換待ち時間は20us必要らしい
#define PERIOD_MIN 		65L			//最大の音の高さの半周期の逆数(us)。125で4kHz
#define REPEAT_LEN_MIN	1397L		//255^2 ÷ 64 +125 + +255
#define POST_SCALE 		16			//timer0は65,536ms毎なので割り込み内でポストスケールを設定する。


#define MODE_PIN		PIN_A3		//スライドボリュームで音が変わるか、自動で音があがっていくか。
#define LED_PIN 		PIN_A4		//状態表示用にＬＥＤ赤緑が交互に点滅する
#define SOUNDOUT_PIN	PIN_A5

//ADCは4つのうち3つ使う
/*
0	繰り返しの周期か、連続モードのときの音の高さ
1	繰り返しの開始の高さ
2	繰り返しの終了の高さ
	

Digital
3	単なる連続音にする
4	動作確認用のＬＥＤの点滅
5	出力

計算上、438Hzから4000Hzまでを2乗曲線で変化するはず。

*/


#fuses	INTRC_IO, NOWDT, PUT, NOPROTECT, NOMCLR, NOBROWNOUT		//3.6VなのでNOBROWNOUTを付けた。
#use Delay(CLOCK=4000000)			//1usを作れるように。

/*0～255を受けて125～1141を返す*/
long set_sound_pitch(int in){
	long pitch;
	pitch = (((long)in * (long)in ) >> 6) + PERIOD_MIN + (long)in;		//inの2乗÷64+125とＨｉになると変化が少ないので＋ｉｎ
	return pitch;
}

/* ADCの読み込み*/
int adc0,adc1,adc2;
void read_adc_all()
{
	set_adc_channel(0);
	delay_us(ADC_WAIT_US);
	adc0 = read_adc();
	set_adc_channel(1);
	delay_us(ADC_WAIT_US);
	adc1 = read_adc();
	set_adc_channel(2);
	delay_us(ADC_WAIT_US);
	adc2 = read_adc();
}

/* 繰り返しの始まり、終わりの位置と、方向と繰り返しの長さを決める */
int repeat_start,repeat_stop;
long repeat_length;
int sound_step;
long sound_step_pitch;
void set_repeat_param(){
	long len_calc;
	if(adc1 == adc2){
		repeat_start = 0;
		repeat_stop = 255;
		sound_step_pitch = sound_step;
	}else if(adc1 < adc2){
		repeat_start = adc1;
		repeat_stop = adc2;
		sound_step_pitch = sound_step;
	}else{
		repeat_start = adc2;
		repeat_stop = adc1;
		sound_step_pitch = 255 - sound_step;
	}
	len_calc = ((long)adc0 << 6)  + REPEAT_LEN_MIN;	// 16320 + 1142 to 1142 …8.35秒～2ms
	repeat_length = len_calc / set_sound_pitch(sound_step);
}

long sound_period;		//引数で受けるか迷った結果、globalにしたけど、分かりづらいかなぁ。
/* 音を出力する */
void toggle_soundpin(){
	delay_us(sound_period);
	output_toggle(SOUNDOUT_PIN);
}

/* 割り込みでＬＥＤの赤緑を点滅させる */
int1 led_rg;
int postscale;
int interval;

#INT_RTCC							//タイマ０割込みの指定
void rtcc_isr() {					//タイマ０割込み処理関数
	if(postscale == 0){				//POST_SCALE 16、interval 255 のとき、1.048sec毎に通過する。
		output_bit(LED_PIN, led_rg);
		led_rg = ~led_rg;
		postscale = POST_SCALE;
	}else{
		postscale--;
	}
	interval = (led_rg) ? adc1 : adc2;
	set_timer0(255-interval);		//小さい方が音が高く、255に近い方が割り込み間隔が短い。
}


void main() {
	
	long counter;
	port_a_pullups(0b00000000);
	set_tris_a(0b00001111);
	setup_adc_ports(sAN0 | sAN1 | sAN2 | VSS_VDD);	
	setup_adc(ADC_CLOCK_DIV_8);
	read_adc_all();
	set_repeat_param();
	
	setup_counters(RTCC_INTERNAL , RTCC_DIV_256);//タイマ０のモード設定。プリスケーラは最大の256。これで65.535ms毎に割り込み発生。
	led_rg = true;
	interval = adc1;
	set_timer0(255-interval);		//小さい方が音が高く、255に近い方が割り込み間隔が短い。
	postscale = POST_SCALE;			//timer1だと16ビットのタイマを使用できるが、adc1,adc2を3項演算子からビットシフトするのが面倒だった。
	enable_interrupts(INT_TIMER0);		//タイマ０割込み許可
	enable_interrupts(GLOBAL);			//グローバル割込み許可
	
	sound_step = 255;
	while(TRUE){
		if(input(MODE_PIN)){				//モードピンがONだったら
			read_adc_all();
			sound_period = set_sound_pitch(adc0);
			toggle_soundpin();	//サウンドピンを変化させる
		}else{
			read_adc_all();					//イコールになったときに永遠に抜け出せなくなるのでここでも読む
			set_repeat_param();
			for (sound_step = repeat_start; sound_step < repeat_stop ; sound_step ++){
				sound_period = set_sound_pitch(sound_step_pitch);
				for(counter = 0; counter < repeat_length; counter ++){
					read_adc_all();					//一定遅延のためここに移動
					set_repeat_param();
					toggle_soundpin();
					if(input(MODE_PIN)){ goto _EXIT;}	//モードピンが変わっていたら一気にループを抜ける
				}
			}
		}
		_EXIT:;
	}
}
