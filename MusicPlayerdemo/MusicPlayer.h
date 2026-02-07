#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QWidget>
#include <QStandardItemModel>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMediaMetaData>

QT_BEGIN_NAMESPACE
namespace Ui { class MusicPlayer; }
QT_END_NAMESPACE

class MusicPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit MusicPlayer(QWidget *parent = nullptr);
    ~MusicPlayer();

private slots:
    void on_opendirbtn_clicked();
    void on_playbtn_clicked();
    void on_nextbtn_clicked();
    void on_prevbtn_clicked();

    // 用 activated，而不是 doubleClicked
    void on_MusicList_activated(const QModelIndex &index);

    void on_playMode_clicked();

    void on_Lyricsbtn_clicked();

private:
    void playMusicByIndex(int index);
    void updateSelection(int index);
    void initDatabase();      // 初始化数据库并建表
    void savePlaylistToDb();  // 将当前 m_musicFiles 保存到数据库
    void loadPlaylistFromDb();// 启动时从数据库读取
    void updateModeUI();
    void loadExternalCover();
    void updateCurrentMusicInfo();
protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MusicPlayer *ui;

    QStandardItemModel *m_listmodel;
    QMediaPlayer *m_mediaPlayer;
    QAudioOutput *m_audioOutput;
    QMap<int,QString> m_lyrics;
    void parseLyric(const QString &lyricFilePath);//解析LRC文件


    QStringList m_musicFiles;
    int m_currentIndex = -1;
    const QString DB_NAME = "music_data.db";
    enum PlayMode { ListLoop, SingleLoop, RandomPlay };
    PlayMode m_currentMode = ListLoop; // 默认列表循环
};

#endif // MUSICPLAYER_H
