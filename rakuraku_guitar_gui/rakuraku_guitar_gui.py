import PySimpleGUI as sg
import sys
import requests
import re
import os

url = "http://192.168.1.99:10001"
name = "team_SOTA"

conversion_table = [[11,"Caug"],
                    [12,"Cdim"],
                    [13,"CM"],
                    [14,"Cm"],
                    [15,"C7"],
                    [16,"Csus4"],
                    [17,"CM7"],
                    [18,"Cm7"],
                    [19,"C7sus4"],
                    [21,"C#aug"],
                    [22,"C#dim"],
                    [23,"C#M"],
                    [24,"C#m"],
                    [25,"C#7"],
                    [26,"C#sus4"],
                    [27,"C#M7"],
                    [28,"C#m7"],
                    [29,"C#7sus4"],
                    [31,"Daug"],
                    [32,"Ddim"],
                    [33,"DM"],
                    [34,"Dm"],
                    [35,"D7"],
                    [36,"Dsus4"],
                    [37,"DM7"],
                    [38,"Dm7"],
                    [39,"D7sus4"],
                    [41,"D#aug"],
                    [42,"D#dim"],
                    [43,"D#M"],
                    [44,"D#m"],
                    [45,"D#7"],
                    [46,"D#sus4"],
                    [47,"D#M7"],
                    [48,"D#m7"],
                    [49,"D#7sus4"],
                    [51,"Eaug"],
                    [52,"Edim"],
                    [53,"EM"],
                    [54,"Em"],
                    [55,"E7"],
                    [56,"Esus4"],
                    [57,"EM7"],
                    [58,"Em7"],
                    [59,"E7sus4"],
                    [61,"Faug"],
                    [62,"Fdim"],
                    [63,"FM"],
                    [64,"Fm"],
                    [65,"F7"],
                    [66,"Fsus4"],
                    [67,"FM7"],
                    [68,"Fm7"],
                    [69,"F7sus4"],
                    [71,"F#aug"],
                    [72,"F#dim"],
                    [73,"F#M"],
                    [74,"F#m"],
                    [75,"F#7"],
                    [76,"F#sus4"],
                    [77,"F#M7"],
                    [78,"F#m7"],
                    [79,"F#7sus4"],
                    [81,"Gaug"],
                    [82,"Gdim"],
                    [83,"GM"],
                    [84,"Gm"],
                    [85,"G7"],
                    [86,"Gsus4"],
                    [87,"GM7"],
                    [88,"Gm7"],
                    [89,"G7sus4"],
                    [91,"G#aug"],
                    [92,"G#dim"],
                    [93,"G#M"],
                    [94,"G#m"],
                    [95,"G#7"],
                    [96,"G#sus4"],
                    [97,"G#M7"],
                    [98,"G#m7"],
                    [99,"G#7sus4"],
                    [101,"Aaug"],
                    [102,"Adim"],
                    [103,"AM"],
                    [104,"Am"],
                    [105,"A7"],
                    [106,"Asus4"],
                    [107,"AM7"],
                    [108,"Am7"],
                    [109,"A7sus4"],
                    [111,"A#aug"],
                    [112,"A#dim"],
                    [113,"A#M"],
                    [114,"A#m"],
                    [115,"A#7"],
                    [116,"A#sus4"],
                    [117,"A#M7"],
                    [118,"A#m7"],
                    [119,"A#7sus4"],
                    [121,"Baug"],
                    [122,"Bdim"],
                    [123,"BM"],
                    [124,"Bm"],
                    [125,"B7"],
                    [126,"Bsus4"],
                    [127,"BM7"],
                    [128,"Bm7"],
                    [129,"B7sus4"]]

def make_data(txt_data):
    send_data = ''
    reshape_data = []
    # 正規表現を使ってコード文字列だけを抽出
    extract_data = re.findall(r'[A-Z][M,a-z,7,#]*', txt_data)
    # Mを追加
    for s in extract_data:
        if len(re.findall(r'[M,a-z,7]', s)) == 0:
            reshape_data.append(s + 'M')
        else:
            reshape_data.append(s)
    
    # 完全一致検索を使ってコードを番号に変換
    for s in reshape_data:
        for c in conversion_table:
            if c[1] == s:
                send_data = send_data + str(c[0]) + '_'
    return reshape_data, send_data


def Window1():
    # 画面レイアウト
    col1 = [[sg.Image(filename=resource_path('img1.png'))]]
    col2 = [[sg.Button('はじめる' ,font=('yu Gothic UI',18),size=(12,1)), sg.Button('終了', font=('yu Gothic UI',18),size=(12,1))]]

    layout1 = [[sg.Column(col1, justification='c')],
               [sg.Column(col2, justification='c')]]

    # １番目の画面を開く
    window = sg.Window("らくラクギター chord sender", layout1, resizable=True, size=(600, 600))
    event1, values1 = window.read()
    if event1 == '終了':
        sys.exit()
    if event1 == 'はじめる':
        pass
    window.close()
    return event1, values1

def Window2():
    # 画面レイアウト
    col1 = [[sg.Image(filename=resource_path('img2.png'))]]
    col2 = [[sg.Text('PC と らくラクギターをWi-Fiで接続してください',font=('yu Gothic UI',18),size=(34,1))]]
    col3 = [[sg.Text('SSID : linksys   PATH : 0123456789',font=('yu Gothic UI',12),size=(28,1))]]
    col4 = [[sg.Button('OK' ,font=('yu Gothic UI',18),size=(12,1)), sg.Button('終了', font=('yu Gothic UI',18),size=(12,1))]]

    layout2 = [[sg.Column(col1, justification='c')],
               [sg.Column(col2, justification='c')],
               [sg.Column(col3, justification='c')],
               [sg.Column(col4, justification='c')]]

    # 2番目の画面を開く
    window = sg.Window("らくラクギター chord sender", layout2, resizable=True, size=(600, 600))
    event2, values2 = window.read()
    if event2 == '終了':
        sys.exit()
    if event2 == 'OK':
        pass
    window.close()
    return event2, values2

def Window3():

    col1 = [[sg.Text('らくラクギターに送信するコード譜を選択してください',font=('yu Gothic UI',18),size=(48,1))]]
    col2 = [[sg.Text("ファイル",font=('yu Gothic UI',18),size=(6,1)), sg.InputText(font=('yu Gothic UI',12),size=(38,1)), sg.FileBrowse(key="file1",font=('yu Gothic UI',18),size=(6,1))]]
    col3 = [[sg.Submit("決定",font=('yu Gothic UI',18),size=(12,1)), sg.Cancel("キャンセル",font=('yu Gothic UI',18),size=(12,1))]]
    

    layout3 = [[sg.Column(col1, justification='l')],
               [sg.Column(col2, justification='l')],
               [sg.Column(col3, justification='c')]]
    # 3番目の画面を開く
    window = sg.Window("らくラクギター chord sender", layout3, resizable=True, size=(600, 600))
    event3, values3 = window.read()
    window.close()
    return event3, values3


def Window4(txt_filepath):

    # txtファイルを開いてデータの読み取り
    f = open(txt_filepath, 'r', encoding='UTF-8')
    txt_data = f.read()
    print(txt_data)
    reshape_data, send_data = make_data(txt_data)
    f.close()

    col1 = [[sg.Text('このコード譜を送信しますか？',font=('yu Gothic UI',18),size=(24,1))]]
    col2 = [[sg.Text(txt_filepath, text_color='black', background_color='white', font=('yu Gothic UI',12),size=(54,1))]]
    col3 = [[sg.Column([[sg.Text(txt_data, text_color='black', background_color='white')]], size=(550, 250), scrollable=True, background_color='white')]]
    col4 = [[sg.Button('Spresenseに送信' ,font=('yu Gothic UI',18),size=(20,1)), sg.Button('キャンセル', font=('yu Gothic UI',18),size=(20,1))]]

    layout4 = [[sg.Column(col1, justification='c')],
               [sg.Column(col2, justification='c')],
               [sg.Column(col3, justification='c')],
               [sg.Column(col4, justification='c')]]

    # layout4 = [[sg.Text('送信しますか')],
    #            [sg.Text(txt_filepath)],
    #            [sg.Text(txt_filepath, text_color='black', background_color='white')],
    #            [sg.Column([[sg.Text(txt_data, text_color='black', background_color='white')]], size=(245, 115), scrollable=True, background_color='white')],
    #            [sg.Column([[sg.Text(reshape_data, text_color='black', background_color='white')]], size=(245, 115), scrollable=True, background_color='white')],
    #            [sg.Column([[sg.Text(send_data, text_color='black', background_color='white')]], size=(245, 115), scrollable=True, background_color='white')],
    #            [sg.Button('Spresenseに送信'), sg.Button('キャンセル')]]
    # 4番目の画面を開く
    window = sg.Window("らくラクギター chord sender", layout4, resizable=True, size=(600, 600))
    event4, values4 = window.read()
    if event4 == 'キャンセル':
        pass
        # sys.exit()
    if event4 == 'Spresenseに送信':
        spr = requests.post(url, data=send_data, timeout=(4.0,7.5))
    window.close()
    return event4, values4

def Window5():
    col1 = [[sg.Text('送信しました',font=('yu Gothic UI',18),size=(10,1))]]
    col2 = [[sg.Button('OK' ,font=('yu Gothic UI',18),size=(20,1))]]

    layout5 = [[sg.Column(col1, justification='c')],
               [sg.Column(col2, justification='c')]]

    # 5番目の画面を開く
    window = sg.Window("らくラクギター chord sender", layout5, resizable=True, size=(600, 600))
    event5, values5 = window.read()
    window.close()
    return event5, values5

def resource_path(relative_path):
    if hasattr(sys, '_MEIPASS'):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath("."), relative_path)
    

def main():
    sg.theme('LightBrown4')
    event1, values1 = Window1()
    print('event1*****')
    print(event1)
    print('values1*****')
    print(values1)
    event2, values2 = Window2()
    print('event2*****')
    print(event2)
    print('values2*****')
    print(values2)
    event3, values3 = Window3()
    print('event3*****')
    print(event3)
    print('values3*****')
    print(values3)
    event4, values4 = Window4(values3['file1'])
    print('event4*****')
    print(event4)
    print('values4*****')
    print(values4)
    event5, values5 = Window5()
    print('event5*****')
    print(event5)
    print('values5*****')
    print(values5)


if __name__ == "__main__":
    main()