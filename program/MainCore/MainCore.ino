//////////////////////////////  Main Core //////////////////////////////////////////////////////////////////////////////////////////
#ifdef SUBCORE  // SubCore指定時にはエラー
#error "Core selection is wrong!!"
#endif

#include <MP.h>
int subcore1 = 1;
int subcore2 = 2; 
int subcore3 = 3; 

#include <SDHCI.h>
#include <Audio.h>
#include <vector>
#include <stdint.h>

AudioClass *theAudio;
File myFile;
bool ErrEnd = false;
// error callback
static void audio_attention_cb(const ErrorAttentionParam *atprm)
{
    printf("************Attention!!!!!**********\n");
    //theAudio->stopPlayer(AudioClass::Player0);
    if (atprm->error_code == AS_ATTENTION_CODE_INFORMATION)
    {
        printf("*****************INFORMATION*******************\n");
        ErrEnd = true;
    }
    if (atprm->error_code == AS_ATTENTION_CODE_WARNING)
    {
        printf("****************WARNING*******************\n");
        ErrEnd = true;
    }
    if (atprm->error_code == AS_ATTENTION_CODE_ERROR)
    {
        printf("*****************ERROR*******************\n");
        ErrEnd = true;
    }
    if (atprm->error_code == AS_ATTENTION_CODE_FATAL)
    {
        printf("*****************FATAL*******************\n");
        ErrEnd = true;
    }
}

class PlayGuitar
{
    public:
        // wavファイル読込用
        struct Item 
        {
            int16_t note;
            String path;
        };
        
        PlayGuitar(const Item *table, size_t table_length);
        void setup();
        void begin();
        void end();
        bool sendNoteOn(int16_t note,int16_t volume);
        void sendNoteOff();
        //void sendNoteEnd();
        void update();
    private:
        // class
        SDClass sd;
        WavContainerFormatParser theParser;
        // wavの1つのデータ
        struct PcmData 
        {
            int16_t note;//番号
            String path;
            uint32_t offset;
            uint32_t size;
        };
        std::vector<PcmData> pcm_table_;
        // param
        int samplingrate = 192000;
        AsClkMode AS_CLKMODE = AS_CLKMODE_HIRES;//AS_CLKMODE_NORMAL or AS_CLKMODE_HIRES
        int bit = 16;//16 or 24
        int channel = 1;
        int max_buffer = 24000;
        uint8_t s_buffer[720];
        uint8_t sc_store_frames = 10;// 最初に読み込むframe数
        uint8_t sc_store_frames_loop = 1;//loopで読み込むframe数

        // sendNoteOn Off End updateで使う
        size_t supply_size = 0;
        uint32_t s_remain_size = 0;
        bool started = false;
        bool carry_over = false;
        bool file_open(int16_t note);
};
// wavのnote,pathをpcm_table_に書き込む
PlayGuitar::PlayGuitar(const Item *table, size_t table_length)
{
    printf("PlayGuitar Constructor\n");
    for (size_t i = 0; i < table_length; i++) 
    {
        PcmData pcm_data;
        pcm_data.note = table[i].note;
        pcm_data.path = table[i].path;
        pcm_table_.push_back(pcm_data);
    }
}
// wavのoffsetとsizeをpcm_table_に書き込む
void PlayGuitar::setup()
{
    printf("PlayGuitar setup\n");
    while (!sd.begin())
    {
        printf("please Insert SD card");
    }
    for (size_t i = 0; i < pcm_table_.size(); i++) 
    {
        fmt_chunk_t fmt;
        // sd cardからWavContainerFormatParserでwav情報読み込////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////み
        handel_wav_parser_t *handle = (handel_wav_parser_t *)theParser.parseChunk(("/mnt/sd0/"+pcm_table_[i].path).c_str(), &fmt);
        if (handle == NULL)
        {
           printf("Wav parser error note %d\n",pcm_table_[i].note);
           exit(1);
        }

        printf("framerate %d = %d\n",i,fmt.rate);
        printf("bit %d = %d\n",i,fmt.bit);
        pcm_table_[i].offset = handle->data_offset;
        pcm_table_[i].size = handle->data_size;
        theParser.resetParser((handel_wav_parser *)handle);
    }
    printf("note,       path, offset, size\n");
    for (size_t i = 0; i < pcm_table_.size(); i++) 
    {
        printf("%3d, %s, %d, %d\n",pcm_table_[i].note, pcm_table_[i].path.c_str(), pcm_table_[i].offset,pcm_table_[i].size);
    }
}
//Audioの初期設定
void PlayGuitar::begin()
{
    theAudio = AudioClass::getInstance();
    theAudio->begin(audio_attention_cb);// HWとの接続
    theAudio->setRenderingClockMode(AS_CLKMODE);
    theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, max_buffer,max_buffer);// 出力先,player1 maxbuffer,player2 maxbuffer
                                                                                       //sampling,16or24,mono or stereo
    err_t err = theAudio->initPlayer(AudioClass::Player0, AS_CODECTYPE_WAV, "/mnt/sd0/BIN",samplingrate,bit,channel);

    if (err != AUDIOLIB_ECODE_OK)
    {
      printf("Player0 initialize error\n");
      exit(1);
    }
    printf("initialization Audio Library\n");
}
// noteからwavファイルを検索し、
// wavファイルのoffsetの位置から読み込み、s_remain_sizeをset
bool PlayGuitar::file_open(int16_t note)
{
    // noteからwavファイルを検索
    int table_index_ = -1;
    for (size_t i = 0; i < pcm_table_.size(); i++) 
    {
        if (note == pcm_table_[i].note) 
        {
            table_index_ = i;
            break;
        }
    }
    myFile.close();
    if (table_index_ < 0) 
    {
        printf("error: note is nothing(note=%d)\n", note);
        return false;
    }
    //printf("play %s"pcm_table_[table_index_].path.c_str());
    myFile = sd.open(pcm_table_[table_index_].path.c_str());
    if (!myFile) 
    {
        printf("error: file open error(%s)\n", pcm_table_[table_index_].path.c_str());
        return false;
    }
    //offsetの位置から読込
    //wavファイルのsizeをset
    myFile.seek(pcm_table_[table_index_].offset);
    s_remain_size = pcm_table_[table_index_].size;
    return true;
}
// note番号と一致するものを最初のフレーム分、読み取る
// volumeも決まる
// started をset
bool PlayGuitar::sendNoteOn(int16_t note,int16_t volume)
{
    // reset
    supply_size = 0;
    carry_over = false;
    
    // 開始時のみファイルの読み込み
    if(!file_open(note))
    {
        printf("file can't read");
        return false;
    }
    // 入力が変化したらここのloopから抜け出せると良い
    for (uint8_t i = 0; i < sc_store_frames; i++)
    {
        supply_size = myFile.read(s_buffer, (s_remain_size < sizeof(s_buffer)) ? s_remain_size : sizeof(s_buffer));
        s_remain_size -= supply_size;
        

        //Serial.print("remain_size=");  
        //Serial.println(s_remain_size);

        int err = theAudio->writeFrames(AudioClass::Player0, s_buffer, supply_size);
        //Serial.print("first write");  
        //  開始時のみチェック
        if (err != AUDIOLIB_ECODE_OK)
        {
            Serial.println("SUDIOLIB_ECODE_OK");
            break;
        }
        //  終了
        if (s_remain_size == 0)
        {
            break;
        }
    }
    theAudio->setVolume(volume);  
    theAudio->startPlayer(AudioClass::Player0);
    started = true;
    //puts("Play!");
    return true;
}
// startedの時、数フレーム分読み取る
// s_remain_sizeが0になったら終了し、sendNoteOff(startedをreset)
void PlayGuitar::update()
{
    if(started)
    {
        for (uint8_t i = 0; i < sc_store_frames_loop; i++)
        {
            if(!carry_over)
            {
                supply_size = myFile.read(s_buffer, (s_remain_size < sizeof(s_buffer)) ? s_remain_size : sizeof(s_buffer));
                s_remain_size -= supply_size;
            }
            carry_over = false;
            //sbufferのsupply_size（読み取れた量）をFIFOに変換
            int err = theAudio->writeFrames(AudioClass::Player0, s_buffer, supply_size);

            // bufferが満タン
            if (err == AUDIOLIB_ECODE_SIMPLEFIFO_ERROR)
            {
                //Serial.println("SUDIOLIB_ECODE_SIMPLEFIFO_ERROR");
                carry_over = true;
                break;
            }
            else
            {
                //Serial.println("not full");
            }
            //Serial.println("s_remain_size =");
            //Serial.println(s_remain_size);
            // 終了
            if (s_remain_size == 0)
            {
                //Serial.print("remain_size =0");
                sendNoteOff();
                break;
            }
        }
    }
}
// 再生をstopし、startedをreset
void PlayGuitar::sendNoteOff()
{
    if(started)
    {
        theAudio->stopPlayer(AudioClass::Player0,AS_STOPPLAYER_NORMAL);
        started = false;
    }
}
//void PlayGuitar::sendNoteEnd()
//{
//    if(started)
//    {
//        theAudio->stopPlayer(AudioClass::Player0);
//        started = false;
//    }
//}
// 終了
void PlayGuitar::end()
{
    theAudio->setReadyMode();
    theAudio->end();
    printf("Exit player\n");
    exit(1);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const PlayGuitar::Item table[437] = {
    {999,"A/free_u.wav"}, // setup.wav
    {-1,"A/free_u.wav"}, // A/free_u.wav
    {-11,"A/Caug_u.wav"},
    {-12,"A/Cdim_u.wav"},
    {-13,"A/CM_u.wav"},
    {-14,"A/Cm__u.wav"},
    {-15,"A/C7_u.wav"},
    {-16,"A/Csus4_u.wav"},
    {-17,"A/CM7_u.wav"},
    {-18,"A/Cm7__u.wav"},
    {-19,"A/C7sus4_u.wav"},
    {-21,"A/C#aug_u.wav"},
    {-22,"A/C#dim_u.wav"},
    {-23,"A/C#M_u.wav"},
    {-24,"A/C#m__u.wav"},
    {-25,"A/C#7_u.wav"},
    {-26,"A/C#sus4_u.wav"},
    {-27,"A/C#M7_u.wav"},
    {-28,"A/C#m7__u.wav"},
    {-29,"A/C#7sus4_u.wav"},
    {-31,"A/Daug_u.wav"},
    {-32,"A/Ddim_u.wav"},
    {-33,"A/DM_u.wav"},
    {-34,"A/Dm__u.wav"},
    {-35,"A/D7_u.wav"},
    {-36,"A/Dsus4_u.wav"},
    {-37,"A/DM7_u.wav"},
    {-38,"A/Dm7__u.wav"},
    {-39,"A/D7sus4_u.wav"},
    {-41,"A/D#aug_u.wav"},
    {-42,"A/D#dim_u.wav"},
    {-43,"A/D#M_u.wav"},
    {-44,"A/D#m__u.wav"},
    {-45,"A/D#7_u.wav"},
    {-46,"A/D#sus4_u.wav"},
    {-47,"A/D#M7_u.wav"},
    {-48,"A/D#m7__u.wav"},
    {-49,"A/D#7sus4_u.wav"},
    {-51,"A/Eaug_u.wav"},
    {-52,"A/Edim_u.wav"},
    {-53,"A/EM_u.wav"},
    {-54,"A/Em__u.wav"},
    {-55,"A/E7_u.wav"},
    {-56,"A/Esus4_u.wav"},
    {-57,"A/EM7_u.wav"},
    {-58,"A/Em7__u.wav"},
    {-59,"A/E7sus4_u.wav"},
    {-61,"A/Faug_u.wav"},
    {-62,"A/Fdim_u.wav"},
    {-63,"A/FM_u.wav"},
    {-64,"A/Fm__u.wav"},
    {-65,"A/F7_u.wav"},
    {-66,"A/Fsus4_u.wav"},
    {-67,"A/FM7_u.wav"},
    {-68,"A/Fm7__u.wav"},
    {-69,"A/F7sus4_u.wav"},
    {-71,"A/F#aug_u.wav"},
    {-72,"A/F#dim_u.wav"},
    {-73,"A/F#M_u.wav"},
    {-74,"A/F#m__u.wav"},
    {-75,"A/F#7_u.wav"},
    {-76,"A/F#sus4_u.wav"},
    {-77,"A/F#M7_u.wav"},
    {-78,"A/F#m7__u.wav"},
    {-79,"A/F#7sus4_u.wav"},
    {-81,"A/Gaug_u.wav"},
    {-82,"A/Gdim_u.wav"},
    {-83,"A/GM_u.wav"},
    {-84,"A/Gm__u.wav"},
    {-85,"A/G7_u.wav"},
    {-86,"A/Gsus4_u.wav"},
    {-87,"A/GM7_u.wav"},
    {-88,"A/Gm7__u.wav"},
    {-89,"A/G7sus4_u.wav"},
    {-91,"A/G#aug_u.wav"},
    {-92,"A/G#dim_u.wav"},
    {-93,"A/G#M_u.wav"},
    {-94,"A/G#m__u.wav"},
    {-95,"A/G#7_u.wav"},
    {-96,"A/G#sus4_u.wav"},
    {-97,"A/G#M7_u.wav"},
    {-98,"A/G#m7__u.wav"},
    {-99,"A/G#7sus4_u.wav"},
    {-101,"A/Aaug_u.wav"},
    {-102,"A/Adim_u.wav"},
    {-103,"A/AM_u.wav"},
    {-104,"A/Am__u.wav"},
    {-105,"A/A7_u.wav"},
    {-106,"A/Asus4_u.wav"},
    {-107,"A/AM7_u.wav"},
    {-108,"A/Am7__u.wav"},
    {-109,"A/A7sus4_u.wav"},
    {-111,"A/A#aug_u.wav"},
    {-112,"A/A#dim_u.wav"},
    {-113,"A/A#M_u.wav"},
    {-114,"A/A#m__u.wav"},
    {-115,"A/A#7_u.wav"},
    {-116,"A/A#sus4_u.wav"},
    {-117,"A/A#M7_u.wav"},
    {-118,"A/A#m7__u.wav"},
    {-119,"A/A#7sus4_u.wav"},
    {-121,"A/Baug_u.wav"},
    {-122,"A/Bdim_u.wav"},
    {-123,"A/BM_u.wav"},
    {-124,"A/Bm__u.wav"},
    {-125,"A/B7_u.wav"},
    {-126,"A/Bsus4_u.wav"},
    {-127,"A/BM7_u.wav"},
    {-128,"A/Bm7__u.wav"},
    {-129,"A/B7sus4_u.wav"},
    {1,"A/free_d.wav"}, // A/free_d.wav
    {11,"A/Caug_d.wav"},
    {12,"A/Cdim_d.wav"},
    {13,"A/CM_d.wav"},
    {14,"A/Cm__d.wav"},
    {15,"A/C7_d.wav"},
    {16,"A/Csus4_d.wav"},
    {17,"A/CM7_d.wav"},
    {18,"A/Cm7__d.wav"},
    {19,"A/C7sus4_d.wav"},
    {21,"A/C#aug_d.wav"},
    {22,"A/C#dim_d.wav"},
    {23,"A/C#M_d.wav"},
    {24,"A/C#m__d.wav"},
    {25,"A/C#7_d.wav"},
    {26,"A/C#sus4_d.wav"},
    {27,"A/C#M7_d.wav"},
    {28,"A/C#m7__d.wav"},
    {29,"A/C#7sus4_d.wav"},
    {31,"A/Daug_d.wav"},
    {32,"A/Ddim_d.wav"},
    {33,"A/DM_d.wav"},
    {34,"A/Dm__d.wav"},
    {35,"A/D7_d.wav"},
    {36,"A/Dsus4_d.wav"},
    {37,"A/DM7_d.wav"},
    {38,"A/Dm7__d.wav"},
    {39,"A/D7sus4_d.wav"},
    {41,"A/D#aug_d.wav"},
    {42,"A/D#dim_d.wav"},
    {43,"A/D#M_d.wav"},
    {44,"A/D#m__d.wav"},
    {45,"A/D#7_d.wav"},
    {46,"A/D#sus4_d.wav"},
    {47,"A/D#M7_d.wav"},
    {48,"A/D#m7__d.wav"},
    {49,"A/D#7sus4_d.wav"},
    {51,"A/Eaug_d.wav"},
    {52,"A/Edim_d.wav"},
    {53,"A/EM_d.wav"},
    {54,"A/Em__d.wav"},
    {55,"A/E7_d.wav"},
    {56,"A/Esus4_d.wav"},
    {57,"A/EM7_d.wav"},
    {58,"A/Em7__d.wav"},
    {59,"A/E7sus4_d.wav"},
    {61,"A/Faug_d.wav"},
    {62,"A/Fdim_d.wav"},
    {63,"A/FM_d.wav"},
    {64,"A/Fm__d.wav"},
    {65,"A/F7_d.wav"},
    {66,"A/Fsus4_d.wav"},
    {67,"A/FM7_d.wav"},
    {68,"A/Fm7__d.wav"},
    {69,"A/F7sus4_d.wav"},
    {71,"A/F#aug_d.wav"},
    {72,"A/F#dim_d.wav"},
    {73,"A/F#M_d.wav"},
    {74,"A/F#m__d.wav"},
    {75,"A/F#7_d.wav"},
    {76,"A/F#sus4_d.wav"},
    {77,"A/F#M7_d.wav"},
    {78,"A/F#m7__d.wav"},
    {79,"A/F#7sus4_d.wav"},
    {81,"A/Gaug_d.wav"},
    {82,"A/Gdim_d.wav"},
    {83,"A/GM_d.wav"},
    {84,"A/Gm__d.wav"},
    {85,"A/G7_d.wav"},
    {86,"A/Gsus4_d.wav"},
    {87,"A/GM7_d.wav"},
    {88,"A/Gm7__d.wav"},
    {89,"A/G7sus4_d.wav"},
    {91,"A/G#aug_d.wav"},
    {92,"A/G#dim_d.wav"},
    {93,"A/G#M_d.wav"},
    {94,"A/G#m__d.wav"},
    {95,"A/G#7_d.wav"},
    {96,"A/G#sus4_d.wav"},
    {97,"A/G#M7_d.wav"},
    {98,"A/G#m7__d.wav"},
    {99,"A/G#7sus4_d.wav"},
    {101,"A/Aaug_d.wav"},
    {102,"A/Adim_d.wav"},
    {103,"A/AM_d.wav"},
    {104,"A/Am__d.wav"},
    {105,"A/A7_d.wav"},
    {106,"A/Asus4_d.wav"},
    {107,"A/AM7_d.wav"},
    {108,"A/Am7__d.wav"},
    {109,"A/A7sus4_d.wav"},
    {111,"A/A#aug_d.wav"},
    {112,"A/A#dim_d.wav"},
    {113,"A/A#M_d.wav"},
    {114,"A/A#m__d.wav"},
    {115,"A/A#7_d.wav"},
    {116,"A/A#sus4_d.wav"},
    {117,"A/A#M7_d.wav"},
    {118,"A/A#m7__d.wav"},
    {119,"A/A#7sus4_d.wav"},
    {121,"A/Baug_d.wav"},
    {122,"A/Bdim_d.wav"},
    {123,"A/BM_d.wav"},
    {124,"A/Bm__d.wav"},
    {125,"A/B7_d.wav"},
    {126,"A/Bsus4_d.wav"},
    {127,"A/BM7_d.wav"},
    {128,"A/Bm7__d.wav"},
    {129,"A/B7sus4_d.wav"},
    {-1001,"E/free_u.wav"},// E/free_u.wav
    {-1011,"E/Caug_u.wav"},
    {-1012,"E/Cdim_u.wav"},
    {-1013,"E/CM_u.wav"},
    {-1014,"E/Cm__u.wav"},
    {-1015,"E/C7_u.wav"},
    {-1016,"E/Csus4_u.wav"},
    {-1017,"E/CM7_u.wav"},
    {-1018,"E/Cm7__u.wav"},
    {-1019,"E/C7sus4_u.wav"},
    {-1021,"E/C#aug_u.wav"},
    {-1022,"E/C#dim_u.wav"},
    {-1023,"E/C#M_u.wav"},
    {-1024,"E/C#m__u.wav"},
    {-1025,"E/C#7_u.wav"},
    {-1026,"E/C#sus4_u.wav"},
    {-1027,"E/C#M7_u.wav"},
    {-1028,"E/C#m7__u.wav"},
    {-1029,"E/C#7sus4_u.wav"},
    {-1031,"E/Daug_u.wav"},
    {-1032,"E/Ddim_u.wav"},
    {-1033,"E/DM_u.wav"},
    {-1034,"E/Dm__u.wav"},
    {-1035,"E/D7_u.wav"},
    {-1036,"E/Dsus4_u.wav"},
    {-1037,"E/DM7_u.wav"},
    {-1038,"E/Dm7__u.wav"},
    {-1039,"E/D7sus4_u.wav"},
    {-1041,"E/D#aug_u.wav"},
    {-1042,"E/D#dim_u.wav"},
    {-1043,"E/D#M_u.wav"},
    {-1044,"E/D#m__u.wav"},
    {-1045,"E/D#7_u.wav"},
    {-1046,"E/D#sus4_u.wav"},
    {-1047,"E/D#M7_u.wav"},
    {-1048,"E/D#m7__u.wav"},
    {-1049,"E/D#7sus4_u.wav"},
    {-1051,"E/Eaug_u.wav"},
    {-1052,"E/Edim_u.wav"},
    {-1053,"E/EM_u.wav"},
    {-1054,"E/Em__u.wav"},
    {-1055,"E/E7_u.wav"},
    {-1056,"E/Esus4_u.wav"},
    {-1057,"E/EM7_u.wav"},
    {-1058,"E/Em7__u.wav"},
    {-1059,"E/E7sus4_u.wav"},
    {-1061,"E/Faug_u.wav"},
    {-1062,"E/Fdim_u.wav"},
    {-1063,"E/FM_u.wav"},
    {-1064,"E/Fm__u.wav"},
    {-1065,"E/F7_u.wav"},
    {-1066,"E/Fsus4_u.wav"},
    {-1067,"E/FM7_u.wav"},
    {-1068,"E/Fm7__u.wav"},
    {-1069,"E/F7sus4_u.wav"},
    {-1071,"E/F#aug_u.wav"},
    {-1072,"E/F#dim_u.wav"},
    {-1073,"E/F#M_u.wav"},
    {-1074,"E/F#m__u.wav"},
    {-1075,"E/F#7_u.wav"},
    {-1076,"E/F#sus4_u.wav"},
    {-1077,"E/F#M7_u.wav"},
    {-1078,"E/F#m7__u.wav"},
    {-1079,"E/F#7sus4_u.wav"},
    {-1081,"E/Gaug_u.wav"},
    {-1082,"E/Gdim_u.wav"},
    {-1083,"E/GM_u.wav"},
    {-1084,"E/Gm__u.wav"},
    {-1085,"E/G7_u.wav"},
    {-1086,"E/Gsus4_u.wav"},
    {-1087,"E/GM7_u.wav"},
    {-1088,"E/Gm7__u.wav"},
    {-1089,"E/G7sus4_u.wav"},
    {-1091,"E/G#aug_u.wav"},
    {-1092,"E/G#dim_u.wav"},
    {-1093,"E/G#M_u.wav"},
    {-1094,"E/G#m__u.wav"},
    {-1095,"E/G#7_u.wav"},
    {-1096,"E/G#sus4_u.wav"},
    {-1097,"E/G#M7_u.wav"},
    {-1098,"E/G#m7__u.wav"},
    {-1099,"E/G#7sus4_u.wav"},
    {-1101,"E/Aaug_u.wav"},
    {-1102,"E/Adim_u.wav"},
    {-1103,"E/AM_u.wav"},
    {-1104,"E/Am__u.wav"},
    {-1105,"E/A7_u.wav"},
    {-1106,"E/Asus4_u.wav"},
    {-1107,"E/AM7_u.wav"},
    {-1108,"E/Am7__u.wav"},
    {-1109,"E/A7sus4_u.wav"},
    {-1111,"E/A#aug_u.wav"},
    {-1112,"E/A#dim_u.wav"},
    {-1113,"E/A#M_u.wav"},
    {-1114,"E/A#m__u.wav"},
    {-1115,"E/A#7_u.wav"},
    {-1116,"E/A#sus4_u.wav"},
    {-1117,"E/A#M7_u.wav"},
    {-1118,"E/A#m7__u.wav"},
    {-1119,"E/A#7sus4_u.wav"},
    {-1121,"E/Baug_u.wav"},
    {-1122,"E/Bdim_u.wav"},
    {-1123,"E/BM_u.wav"},
    {-1124,"E/Bm__u.wav"},
    {-1125,"E/B7_u.wav"},
    {-1126,"E/Bsus4_u.wav"},
    {-1127,"E/BM7_u.wav"},
    {-1128,"E/Bm7__u.wav"},
    {-1129,"E/B7sus4_u.wav"},
    {1001,"E/free_d.wav"},// E/free_d.wav
    {1011,"E/Caug_d.wav"},
    {1012,"E/Cdim_d.wav"},
    {1013,"E/CM_d.wav"},
    {1014,"E/Cm__d.wav"},
    {1015,"E/C7_d.wav"},
    {1016,"E/Csus4_d.wav"},
    {1017,"E/CM7_d.wav"},
    {1018,"E/Cm7__d.wav"},
    {1019,"E/C7sus4_d.wav"},
    {1021,"E/C#aug_d.wav"},
    {1022,"E/C#dim_d.wav"},
    {1023,"E/C#M_d.wav"},
    {1024,"E/C#m__d.wav"},
    {1025,"E/C#7_d.wav"},
    {1026,"E/C#sus4_d.wav"},
    {1027,"E/C#M7_d.wav"},
    {1028,"E/C#m7__d.wav"},
    {1029,"E/C#7sus4_d.wav"},
    {1031,"E/Daug_d.wav"},
    {1032,"E/Ddim_d.wav"},
    {1033,"E/DM_d.wav"},
    {1034,"E/Dm__d.wav"},
    {1035,"E/D7_d.wav"},
    {1036,"E/Dsus4_d.wav"},
    {1037,"E/DM7_d.wav"},
    {1038,"E/Dm7__d.wav"},
    {1039,"E/D7sus4_d.wav"},
    {1041,"E/D#aug_d.wav"},
    {1042,"E/D#dim_d.wav"},
    {1043,"E/D#M_d.wav"},
    {1044,"E/D#m__d.wav"},
    {1045,"E/D#7_d.wav"},
    {1046,"E/D#sus4_d.wav"},
    {1047,"E/D#M7_d.wav"},
    {1048,"E/D#m7__d.wav"},
    {1049,"E/D#7sus4_d.wav"},
    {1051,"E/Eaug_d.wav"},
    {1052,"E/Edim_d.wav"},
    {1053,"E/EM_d.wav"},
    {1054,"E/Em__d.wav"},
    {1055,"E/E7_d.wav"},
    {1056,"E/Esus4_d.wav"},
    {1057,"E/EM7_d.wav"},
    {1058,"E/Em7__d.wav"},
    {1059,"E/E7sus4_d.wav"},
    {1061,"E/Faug_d.wav"},
    {1062,"E/Fdim_d.wav"},
    {1063,"E/FM_d.wav"},
    {1064,"E/Fm__d.wav"},
    {1065,"E/F7_d.wav"},
    {1066,"E/Fsus4_d.wav"},
    {1067,"E/FM7_d.wav"},
    {1068,"E/Fm7__d.wav"},
    {1069,"E/F7sus4_d.wav"},
    {1071,"E/F#aug_d.wav"},
    {1072,"E/F#dim_d.wav"},
    {1073,"E/F#M_d.wav"},
    {1074,"E/F#m__d.wav"},
    {1075,"E/F#7_d.wav"},
    {1076,"E/F#sus4_d.wav"},
    {1077,"E/F#M7_d.wav"},
    {1078,"E/F#m7__d.wav"},
    {1079,"E/F#7sus4_d.wav"},
    {1081,"E/Gaug_d.wav"},
    {1082,"E/Gdim_d.wav"},
    {1083,"E/GM_d.wav"},
    {1084,"E/Gm__d.wav"},
    {1085,"E/G7_d.wav"},
    {1086,"E/Gsus4_d.wav"},
    {1087,"E/GM7_d.wav"},
    {1088,"E/Gm7__d.wav"},
    {1089,"E/G7sus4_d.wav"},
    {1091,"E/G#aug_d.wav"},
    {1092,"E/G#dim_d.wav"},
    {1093,"E/G#M_d.wav"},
    {1094,"E/G#m__d.wav"},
    {1095,"E/G#7_d.wav"},
    {1096,"E/G#sus4_d.wav"},
    {1097,"E/G#M7_d.wav"},
    {1098,"E/G#m7__d.wav"},
    {1099,"E/G#7sus4_d.wav"},
    {1101,"E/Aaug_d.wav"},
    {1102,"E/Adim_d.wav"},
    {1103,"E/AM_d.wav"},
    {1104,"E/Am__d.wav"},
    {1105,"E/A7_d.wav"},
    {1106,"E/Asus4_d.wav"},
    {1107,"E/AM7_d.wav"},
    {1108,"E/Am7__d.wav"},
    {1109,"E/A7sus4_d.wav"},
    {1111,"E/A#aug_d.wav"},
    {1112,"E/A#dim_d.wav"},
    {1113,"E/A#M_d.wav"},
    {1114,"E/A#m__d.wav"},
    {1115,"E/A#7_d.wav"},
    {1116,"E/A#sus4_d.wav"},
    {1117,"E/A#M7_d.wav"},
    {1118,"E/A#m7__d.wav"},
    {1119,"E/A#7sus4_d.wav"},
    {1121,"E/Baug_d.wav"},
    {1122,"E/Bdim_d.wav"},
    {1123,"E/BM_d.wav"},
    {1124,"E/Bm__d.wav"},
    {1125,"E/B7_d.wav"},
    {1126,"E/Bsus4_d.wav"},
    {1127,"E/BM7_d.wav"},
    {1128,"E/Bm7__d.wav"},
    {1129,"E/B7sus4_d.wav"}
};
/////////////////////////////////////receive code list(Subcore 3)///////////////////////////////////////////////////////
#define MSGLEN      200
struct MyPacket {
    volatile int status; /* 0:ready, 1:busy */
    char message[MSGLEN];
};
std::vector<int16_t> number(char* str)
{
    char *ptr;
    std::vector<int16_t> v;
    v.clear();
    // _を区切りに文字列を分割
    // 1回目
    ptr = strtok(str, "_");
    v.push_back(atoi(ptr));
    //printf("ptr = %s\n",ptr);
    
    // 2回目以降
    while(ptr != NULL) {
        // strtok関数により変更されたNULLのポインタが先頭
        ptr = strtok(NULL, "_");
        
        // ptrがNULLの場合エラーが発生するので対処
        if(ptr != NULL) {
           v.push_back(atoi(ptr));
           //printf("ptr,i =%s,%d\n", ptr,i);
        }
    }
    return v;
}
/////////////////////////////////////////// Main //////////////////////////////////////////////////////////////////////
#define Swich_12 (12)//音源
#define Swich_13 (13)//beginner_mode

PlayGuitar inst(table, 437);
void setup() 
{
    // シリアル通信設定
    Serial.begin(2000000);
    inst.setup();
    inst.begin();

    pinMode(Swich_12, INPUT);
    pinMode(Swich_13, INPUT);

    // マルチコア起動
    int ret1 = 0;
    int ret2 = 0;
    int ret3 = 0;
    ret1 = MP.begin(subcore1);
    ret2 = MP.begin(subcore2);
    ret3 = MP.begin(subcore3);
    MP.RecvTimeout(MP_RECV_POLLING);   
    // 起動エラー
    if (ret1 < 0) 
        printf("MP.begin 1 error = %d\n", ret1);
    if (ret2 < 0) 
        printf("MP.begin 2 error = %d\n", ret2);
    if (ret3 < 0) 
        printf("MP.begin 3 error = %d\n", ret3);
    
    
    printf("start\n");
    inst.sendNoteOn(999,-100);
}

bool speed_up = false;
uint32_t rcvdata2;
long sub2_time = 0;

bool beginner_mode_started = false;
std::vector<int16_t> code_list;
uint16_t list_count = 0;

int16_t note = 10000;
int16_t volume = 0;

void loop()
{
    //////////////////////加速判定 (subcore 2)//////////////////////////
    long now_time = micros();
    double receive_time_million = now_time - sub2_time;
    double receive_time = receive_time_million/1000000;
    if(receive_time>0.08)
    {
        int      ret2;
        int8_t   rcvid;
        MP.RecvTimeout(MP_RECV_POLLING);   
        ret2 = MP.Recv(&rcvid, &rcvdata2, subcore2);  // SubCore2からmsg受信
        if (ret2 < 0) {
            //printf("encorder can");
        }
        else
        {
            sub2_time = micros();
            printf("Encorder data=%d \n", rcvdata2);
            //音量の変更
            if(rcvdata2 == 1||rcvdata2 == 2)
            {
                volume = 50;
            }
            else
            {
                volume = -100;
            }
            speed_up = true;
        }
    }
    ///////////////////////receive code list (subcore 3)//////////////////////////
    MP.RecvTimeout(MP_RECV_POLLING);
    int      ret3;
    int8_t   msgid;
    MyPacket *packet;

    /* Receive message from SubCore */
    ret3 = MP.Recv(&msgid, &packet, subcore3);
    if (ret3 > 0) {
        //printf("%s\n", packet->message);
        char* note_message = packet->message;
        code_list = number(note_message);
        printf("size = %d\n",code_list.size());
        for(int i=0;i< code_list.size();i++)
        {
            printf("%d,",code_list[i]);
        }
        printf("\n");
        /* status -> ready */
        packet->status = 0;
        inst.sendNoteOff();
        inst.sendNoteOn(1001,-100);
    }
    ///////////////////////////////////////////////////////////////
    bool beginner_mode = digitalRead(Swich_13);
    if(beginner_mode_started == true&&beginner_mode == false)
    {
        beginner_mode_started = false;
    }
    if(speed_up)
    {
    //////////// beginner mode ///////////////////////////////////////
        if(beginner_mode)
        {
            printf("beginner mode = %d\n",beginner_mode_started);
            if(code_list.size()>0)
            {
                if(!beginner_mode_started)
                {
                    list_count = 0;
                    note = code_list[list_count];
                    beginner_mode_started = true;
                }
                else
                {
                    list_count = list_count+1;
                    if(list_count == code_list.size())
                    {
                        list_count = 0;
                    }
                    note = code_list[list_count];
                }
            }
        }
    /////////////// code or manual mode (subcore 1)//////////////////////////
        else
        {
            int      ret1;
            uint32_t rcvdata = 1;
            int8_t   rcvid = 1;
            ret1 = MP.Send(rcvid,rcvdata,subcore1);
            if(ret1<0)
                printf("fail connect SubCore1\n");
            else
            {
                //Serial.println("to 1 send success");
                MP.RecvTimeout(MP_RECV_BLOCKING);   
                ret1 = MP.Recv(&rcvid, &rcvdata, subcore1);  // SubCore1からmsg受
                if(ret1<0)
                    printf("can't receive SubCore1\n");
                else
                {
                    note = rcvdata;
                    printf("from SubCore1 data %d\n",note);
                }
            }

        }
    ////////////////// 音を鳴らす //////////////////////
        if(note == 10000)
        {

        }
        else
        {
            // elekiの場合
            bool Acoustic = digitalRead(Swich_12);
            if(!Acoustic)
            {
                note = note + 1000;
            }
            //ダウンストロークの場合
            if(rcvdata2 == 2||rcvdata2 == 3)
            {
                note = -note;
            }
            printf("play_number= %d\n",note);
            inst.sendNoteOff();
            inst.sendNoteOn(note,volume);
        }
        speed_up = false;
    }
    inst.update();
    return;
}
