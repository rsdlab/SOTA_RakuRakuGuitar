///////////////////////// SubCore2 //////////////////////////////////////////////////////////////////
#if (SUBCORE != 2)  // SubCoreの指定
#error "Core selection is wrong!!"
#endif

#include <MP.h>


class Encorder
{
    private: 
    // pin設定と回転方向
        int pinA = 9; //CLK
        int pinB = 10;//DT
        const int CW = 1;
        const int CCW = -1;
    // update で使う 
        int multiplication = 4;//逓倍
        int resolution = 19;//分解能
        int a_now=0;
        int b_now=0;
        int a_pri=0;
        int b_pri=0;
    // calculate_step_dtで使う
        bool first = true;
        double step_angular;
        int count = 0;//エンコーダカウント
        int direction_pri;
    // dtで使う
        double dt[6];
        long t_now =0;
        long t_pri=0;
    // flagで使う(加速度判定)
        double ddt_now;
    // updateとflagで使う（速度判定）
        double flag_time = 0;
        bool velocity_check = false;
        int velocity_count;
    // param
        double threshold = 0.012;           //加速判定閾値0.008
        double flag_to_flag_time = 0.2;     //加速判定後の次の加速までの最低時間[s]
        bool velocity_use = false;          //速度判定行うかどうか
        double check_limit = 0.04;          //加速判定後速度計測にかける時間[s]
        double velocity_count_threshold = 4;//check_limitでhighspeedと判定される最低ステップ数       
    public:
        int section_direction;//
        int theta_now = 0;
        bool highspeed = false;
        Encorder();
        void setup();
        bool update();
        void caluculate_step_dt(int direction_now);
        void dt_latest_update();
        void dt_stock_update();
        void flag();
};
Encorder::Encorder()
{
    step_angular = 360/(resolution*multiplication);
    for(int i=0;i<6;i++)
    {
        dt[i]=0;
    }
}
void Encorder::setup()
{
    pinMode(pinA,INPUT);
    pinMode(pinB,INPUT);
}

void Encorder::caluculate_step_dt(int direction_now)
{
    if(first)
    {
        first = false;
        dt_latest_update();//最新ステップ変化時間を更新
        section_direction = direction_now;
    }
    else
    {
        if(direction_pri != direction_now)// 同じ区間を行き来していたら
        {
            dt_latest_update();//最新ステップ変化時間のみを更新
            section_direction = section_direction + direction_now;
        }
        else
        {
            if(section_direction != 0)// 前回の角度変位が有効であれば（2ステップ同方向に変化した）
            {
                dt_stock_update();
                flag();
            }
            else//　前回の角度変位が無効なら(結果的に現在のステップしか変化していない)
            {
                dt_latest_update();//最新ステップ変化時間のみを更新
            }
            section_direction = direction_now;
        }
    }
    direction_pri = direction_now;
    //count = count + direction_now;
    //theta_now = count*step_angular;
}

bool Encorder::update()
{
    a_now = digitalRead(pinA);
    b_now = digitalRead(pinB);
    if(a_now != a_pri)
    {
        if(a_now != b_now){
            caluculate_step_dt(CW);
        }
        else if(a_now == b_now){             
            caluculate_step_dt(CCW);
        }
    }
    if(b_now != b_pri){
        if(b_now == a_now){
            caluculate_step_dt(CW);
        }
        else if(b_now != a_now){
            caluculate_step_dt(CCW);
        }
    }
    a_pri=a_now;
    b_pri=b_now;

    if(velocity_use)
    {
        if(velocity_check)
        {
            long time_now = micros();
            double check_time_million=time_now-flag_time;
            double check_time = check_time_million/1000000;
            if(check_limit < check_time)
            {
                if(velocity_count < velocity_count_threshold-1)
                {
                    Serial.println("0");
                    highspeed = false;
                }
                else if(velocity_count >= velocity_count_threshold)
                {
                    Serial.println("1");
                    highspeed = true;
                }
                else
                {
                    Serial.println("0.5");
                }
                velocity_check = false;
                return true;
            }
        }
        return false;
    }
    else
    {
        if(velocity_check)
        {
            velocity_check = false;
            return true;
        }
        return false;
    }
}

void Encorder::dt_stock_update()
{
    for(int i=0;i<5;i++)
    {
        dt[i] = dt[i+1];
    }
    t_pri = t_now;
    t_now = micros();
    double dt_million = t_now - t_pri;
    dt[5] = dt_million/1000000;
}

void Encorder::dt_latest_update()
{
    t_now = micros();
    double dt_million = t_now - t_pri;
    dt[5] = dt_million/1000000;
}

void Encorder::flag()
{
    if(!velocity_check)
    {
        ddt_now = (dt[0]-dt[4])+(dt[1]-dt[5]);
        if (ddt_now >= threshold)
        {
            long now_time = micros();
            double flag_to_now_time = (now_time-flag_time)/1000000;
            //Serial.println(flag_to_now_time);
            if(flag_to_now_time > flag_to_flag_time)
            {
                flag_time = micros();
                velocity_check = true;
                velocity_count =0;
                check_limit = check_limit - dt[5];
            }
        }
    }
    else
    {
        velocity_count = velocity_count + 1;
    }
}

Encorder encorder;
void setup()
{
    int ret2 = 0;
    Serial.begin(2000000);
    while (!Serial);
    encorder.setup();
    // マルチコア起動
    ret2 = MP.begin();  
    MP.RecvTimeout(MP_RECV_POLLING);   
    //　起動エラー
    if (ret2 < 0) 
        printf("SubCore start error = %d\n", ret2);
}

void loop()
{
    int ret2;
    int8_t msgid = 0;
    uint32_t msgdata = 0;
    if(encorder.update())
    {
        //Serial.print("flag!!!!!!!!!!!!!!!!!!");
        if(encorder.section_direction>0 && !encorder.highspeed)
            msgdata = 0;
        else if(encorder.section_direction>0 && encorder.highspeed)
            msgdata = 1;    
        else if(encorder.section_direction<0 && encorder.highspeed)
            msgdata = 2;
        else if(encorder.section_direction<0 && !encorder.highspeed)
            msgdata = 3;
        //printf("SubCore2 send: id=%d data=%d\n", msgid, msgdata);
        ret2 = MP.Send(msgid, msgdata);  
        // msg送信エラー
        if (ret2 < 0) 
            printf("MP.Send error = %d\n", ret2);
    }
    return;
}
