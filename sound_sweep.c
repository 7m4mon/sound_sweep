/*
2016�N12��12��
���q�̂��߂ɉ����ς��I���`�������
Memory usage:   ROM=49%      RAM=50% - 72%
*/
#include <12f675.h>					//�A�i���O�|�[�g���g���B
#DEVICE ADC=8

#define ADC_WAIT_US		20			//ADC�̕ϊ��҂����Ԃ�20us�K�v�炵��
#define PERIOD_MIN 		65L			//�ő�̉��̍����̔������̋t��(us)�B125��4kHz
#define REPEAT_LEN_MIN	1397L		//255^2 �� 64 +125 + +255
#define POST_SCALE 		16			//timer0��65,536ms���Ȃ̂Ŋ��荞�ݓ��Ń|�X�g�X�P�[����ݒ肷��B


#define MODE_PIN		PIN_A3		//�X���C�h�{�����[���ŉ����ς�邩�A�����ŉ����������Ă������B
#define LED_PIN 		PIN_A4		//��ԕ\���p�ɂk�d�c�ԗ΂����݂ɓ_�ł���
#define SOUNDOUT_PIN	PIN_A5

//ADC��4�̂���3�g��
/*
0	�J��Ԃ��̎������A�A�����[�h�̂Ƃ��̉��̍���
1	�J��Ԃ��̊J�n�̍���
2	�J��Ԃ��̏I���̍���
	

Digital
3	�P�Ȃ�A�����ɂ���
4	����m�F�p�̂k�d�c�̓_��
5	�o��

�v�Z��A438Hz����4000Hz�܂ł�2��Ȑ��ŕω�����͂��B

*/


#fuses	INTRC_IO, NOWDT, PUT, NOPROTECT, NOMCLR, NOBROWNOUT		//3.6V�Ȃ̂�NOBROWNOUT��t�����B
#use Delay(CLOCK=4000000)			//1us������悤�ɁB

/*0�`255���󂯂�125�`1141��Ԃ�*/
long set_sound_pitch(int in){
	long pitch;
	pitch = (((long)in * (long)in ) >> 6) + PERIOD_MIN + (long)in;		//in��2�恀64+125�Ƃg���ɂȂ�ƕω������Ȃ��̂Ł{����
	return pitch;
}

/* ADC�̓ǂݍ���*/
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

/* �J��Ԃ��̎n�܂�A�I���̈ʒu�ƁA�����ƌJ��Ԃ��̒��������߂� */
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
	len_calc = ((long)adc0 << 6)  + REPEAT_LEN_MIN;	// 16320 + 1142 to 1142 �c8.35�b�`2ms
	repeat_length = len_calc / set_sound_pitch(sound_step);
}

long sound_period;		//�����Ŏ󂯂邩���������ʁAglobal�ɂ������ǁA������Â炢���Ȃ��B
/* �����o�͂��� */
void toggle_soundpin(){
	delay_us(sound_period);
	output_toggle(SOUNDOUT_PIN);
}

/* ���荞�݂łk�d�c�̐ԗ΂�_�ł����� */
int1 led_rg;
int postscale;
int interval;

#INT_RTCC							//�^�C�}�O�����݂̎w��
void rtcc_isr() {					//�^�C�}�O�����ݏ����֐�
	if(postscale == 0){				//POST_SCALE 16�Ainterval 255 �̂Ƃ��A1.048sec���ɒʉ߂���B
		output_bit(LED_PIN, led_rg);
		led_rg = ~led_rg;
		postscale = POST_SCALE;
	}else{
		postscale--;
	}
	interval = (led_rg) ? adc1 : adc2;
	set_timer0(255-interval);		//�������������������A255�ɋ߂��������荞�݊Ԋu���Z���B
}


void main() {
	
	long counter;
	port_a_pullups(0b00000000);
	set_tris_a(0b00001111);
	setup_adc_ports(sAN0 | sAN1 | sAN2 | VSS_VDD);	
	setup_adc(ADC_CLOCK_DIV_8);
	read_adc_all();
	set_repeat_param();
	
	setup_counters(RTCC_INTERNAL , RTCC_DIV_256);//�^�C�}�O�̃��[�h�ݒ�B�v���X�P�[���͍ő��256�B�����65.535ms���Ɋ��荞�ݔ����B
	led_rg = true;
	interval = adc1;
	set_timer0(255-interval);		//�������������������A255�ɋ߂��������荞�݊Ԋu���Z���B
	postscale = POST_SCALE;			//timer1����16�r�b�g�̃^�C�}���g�p�ł��邪�Aadc1,adc2��3�����Z�q����r�b�g�V�t�g����̂��ʓ|�������B
	enable_interrupts(INT_TIMER0);		//�^�C�}�O�����݋���
	enable_interrupts(GLOBAL);			//�O���[�o�������݋���
	
	sound_step = 255;
	while(TRUE){
		if(input(MODE_PIN)){				//���[�h�s����ON��������
			read_adc_all();
			sound_period = set_sound_pitch(adc0);
			toggle_soundpin();	//�T�E���h�s����ω�������
		}else{
			read_adc_all();					//�C�R�[���ɂȂ����Ƃ��ɉi���ɔ����o���Ȃ��Ȃ�̂ł����ł��ǂ�
			set_repeat_param();
			for (sound_step = repeat_start; sound_step < repeat_stop ; sound_step ++){
				sound_period = set_sound_pitch(sound_step_pitch);
				for(counter = 0; counter < repeat_length; counter ++){
					read_adc_all();					//���x���̂��߂����Ɉړ�
					set_repeat_param();
					toggle_soundpin();
					if(input(MODE_PIN)){ goto _EXIT;}	//���[�h�s�����ς���Ă������C�Ƀ��[�v�𔲂���
				}
			}
		}
		_EXIT:;
	}
}
