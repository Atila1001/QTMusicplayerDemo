#include "MusicPlayer.h"
#include "ui_MusicPlayer.h"

#include <QFileDialog>
#include <QDirIterator>
#include <QIcon>
#include <QUrl>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QTimer>
#include <QEventLoop>
#include <QCloseEvent>
#include <QRandomGenerator>
MusicPlayer::MusicPlayer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MusicPlayer)
    , m_listmodel(new QStandardItemModel(this))
    , m_mediaPlayer(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
{
    ui->setupUi(this);

    /* ---------- 列表初始化 ---------- */
    ui->MusicList->setModel(m_listmodel);
    ui->MusicList->setEditTriggers(QAbstractItemView::NoEditTriggers);

    /* ---------- 按钮初始状态 ---------- */
    ui->playbtn->setEnabled(false);
    ui->prevbtn->setEnabled(false);
    ui->nextbtn->setEnabled(false);

    ui->playbtn->setIcon(QIcon(":/Resource/icons8-play-50.png"));
    ui->playbtn->setIconSize(QSize(32, 32));
    ui->volumebtn->setIcon(QIcon(":/Resource/icons8-volume-64.png"));
    ui->playbtn->setToolTip("播放/暂停");
    ui->prevbtn->setToolTip("上一首");
    ui->nextbtn->setToolTip("下一首");
    ui->opendirbtn->setToolTip("添加本地音乐");
    ui->Lyricsbtn->setToolTip("显示/隐藏歌词");
    ui->volumebtn->setToolTip("静音/解除静音");
    ui->volumeSlider->setToolTip("音量调节");
    ui->albumArtLabel->setToolTip("专辑封面");
    ui->MusicList->setToolTip("音乐列表");
    ui->label->setToolTip("歌词");

    /* ---------- 音频 ---------- */
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.5);
    //音量滑动条默认处于中间
    ui->volumeSlider->setValue(50);
    //数据库加载
    initDatabase();
    loadPlaylistFromDb();
    connect(m_mediaPlayer, &QMediaPlayer::metaDataChanged, this, [this]() {
        QMediaMetaData data = m_mediaPlayer->metaData();

        // 优先尝试获取封面图
        QVariant art = data.value(QMediaMetaData::ThumbnailImage);
        if (art.isNull()) {
            art = data.value(QMediaMetaData::CoverArtImage);
        }

        if (art.isValid() && !art.isNull()) {
            QImage img = art.value<QImage>();
            if (!img.isNull()) {
                // 使用 scaled 确保图片不会撑破 Label，保持比例
                ui->albumArtLabel->setPixmap(QPixmap::fromImage(img).scaled(
                    ui->albumArtLabel->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
            }
        }
        // 如果内嵌图解析失败，loadExternalCover 会在 setSource 之前或之后提供兜底图标
    });
    //获取当前媒体的总时长，通过信号关联获取
    connect(m_mediaPlayer,&QMediaPlayer::durationChanged,this,[=](qint64 duration)
            {
                // 1. 确保时长非负，将毫秒转换为总秒数（duration 单位是毫秒）
                qint64 totalSeconds = qMax(duration, 0LL) / 1000;
                // 2. 计算分钟和秒（分离逻辑，更易维护）
                int minutes = static_cast<int>(totalSeconds / 60);
                int seconds = static_cast<int>(totalSeconds % 60);
                // 3. 格式化：分钟和秒均为2位，不足补0
                ui->totalLabel->setText(QString("%1:%2")
                                            .arg(minutes, 2, 10, QChar('0'))  // 分钟：2位、十进制、补0
                                            .arg(seconds, 2, 10, QChar('0'))); // 秒：同上
                ui->playCourseSlider->setRange(0,duration);//拖动进度条
            } );
    //获取当前播放进度对应时间
    connect(m_mediaPlayer,&::QMediaPlayer::positionChanged,this,[=](qint64 positon)
            {
                // 1. 确保时长非负，将毫秒转换为总秒数（duration 单位是毫秒）
                qint64 totalSeconds = qMax(positon, 0LL) / 1000;
                // 2. 计算分钟和秒（分离逻辑，更易维护）
                int minutes = static_cast<int>(totalSeconds / 60);
                int seconds = static_cast<int>(totalSeconds % 60);
                // 3. 格式化：分钟和秒均为2位，不足补0
                ui->curLabel->setText(QString("%1:%2")
                                            .arg(minutes, 2, 10, QChar('0'))  // 分钟：2位、十进制、补0
                                            .arg(seconds, 2, 10, QChar('0'))); // 秒：同上
                ui->playCourseSlider->setValue(positon);//拖动进度条
            } );
    //拖动进度条
    connect(ui->playCourseSlider,&QSlider::sliderMoved,m_mediaPlayer,&QMediaPlayer::setPosition);
    /* ---------- 播放 / 暂停图标 ---------- */
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState state) {
                ui->playbtn->setIcon(
                    state == QMediaPlayer::PlayingState
                        ? QIcon(":/Resource/icons8-hold-64.png")
                        : QIcon(":/Resource/icons8-play-50.png"));
            });
    //音量调节
    connect(ui->volumeSlider,&QSlider::valueChanged,this, [this](int value) {
        // 转换为QAudioOutput要求的0.0~1.0范围
        qreal volume = static_cast<qreal>(value) / 100.0;
        m_audioOutput->setVolume(volume);
        // 新增：实时更新音量标签显示百分比
        ui->volumeLabel->setText(QString("%1%").arg(value));
    });
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred,
            this, [this](QMediaPlayer::Error error, const QString &errorString) {
                QMessageBox::information(this, "", errorString);
            });
    //静音功能
    connect(ui->volumebtn,&QPushButton::clicked,this,[this](){
        static int lastvolume=50;
        //判断当前是否静音
        if(m_audioOutput->volume()>0.0)
            {
            //记录
            lastvolume=ui->volumeSlider->value();
            //音量置0
            m_audioOutput->setVolume(0);
            //滑块置0
            ui->volumeSlider->setValue(0);
            //文本更新
            ui->volumeLabel->setText("0%");
            //静音图标
            ui->volumebtn->setIcon(QIcon(":/Resource/icons8-mute-64.png"));
        }
else{
            // 取消静音，恢复原音量
            // 1. 恢复音频音量
            m_audioOutput->setVolume(static_cast<qreal>(lastvolume) / 100.0);
            // 2. 滑块恢复原位置
            ui->volumeSlider->setValue(lastvolume);
            // 3. 标签恢复原百分比
            ui->volumeLabel->setText(QString("%1%").arg(lastvolume));
            // 4. 切换为非静音图标
            ui->volumebtn->setIcon(QIcon(":/Resource/icons8-volume-64.png"));        }
    });
    // 选中变更时同步 m_currentIndex（支持单击、键盘、程序设置）
    if (ui->MusicList->selectionModel()) {
        connect(ui->MusicList->selectionModel(),
                &QItemSelectionModel::currentChanged,
                this,
                [this](const QModelIndex &current, const QModelIndex & /*previous*/) {
                    if (current.isValid()) {
                        m_currentIndex = current.row();
                    }
                });
    }
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status){
        if (status == QMediaPlayer::EndOfMedia) {
            if (m_currentMode == SingleLoop) {
                // 如果是单曲循环，播完了直接重播当前索引
                playMusicByIndex(m_currentIndex);
            } else {
                // 列表循环或随机播放，则直接调用下一曲逻辑
                on_nextbtn_clicked();
            }
        }
    });
    // 歌词显示
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (m_lyrics.isEmpty()) return;

        // 寻找小于当前位置的最大时间戳
        auto it = m_lyrics.upperBound(position);
        if (it != m_lyrics.begin()) {
            it--;
            ui->label->setText(it.value()); // 在 page_2 的 label 上显示当前歌词
        }
    });
    m_mediaPlayer->stop();
}

MusicPlayer::~MusicPlayer()
{
    delete ui;
}
/* ---------------- 数据库相关实现 ---------------- */

void MusicPlayer::initDatabase()
{
    // 添加 SQLite 驱动并设置数据库名
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("music_config.db");

    if (!db.open()) {
        QMessageBox::warning(this, "数据库错误", "无法打开配置文件: " + db.lastError().text());
        return;
    }

    QSqlQuery query;
    // 创建播放列表表
    query.exec("CREATE TABLE IF NOT EXISTS playlist (id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT UNIQUE, name TEXT)");
    // 创建播放状态表（存储最后播放的歌曲索引和进度）
    query.exec("CREATE TABLE IF NOT EXISTS play_state (key TEXT PRIMARY KEY, value TEXT)");
}

void MusicPlayer::loadPlaylistFromDb() {

    QSqlQuery query("SELECT path, name FROM playlist");
    while (query.next()) {
        QString path = query.value(0).toString();
        if (QFile::exists(path)) {
            m_musicFiles << path;
            m_listmodel->appendRow(new QStandardItem(query.value(1).toString()));
        }
    }

    if (!m_musicFiles.isEmpty()) {
        ui->playbtn->setEnabled(true);
        ui->prevbtn->setEnabled(true);
        ui->nextbtn->setEnabled(true);

        QSqlQuery stateQuery("SELECT value FROM play_state WHERE key='lastIndex'");
        if (stateQuery.next()) {
            int lastIndex = stateQuery.value(0).toInt();
            if (lastIndex >= 0 && lastIndex < m_musicFiles.size()) {
                m_currentIndex = lastIndex;
                updateSelection(m_currentIndex);

                // 1. 设置播放源（即使不播放也要设置，否则拿不到内嵌封面）
                m_mediaPlayer->setSource(QUrl::fromLocalFile(m_musicFiles.at(m_currentIndex)));
                // 2. 加载封面和歌词
                updateCurrentMusicInfo();
            }
        }
    }
    QSqlQuery modeQuery("SELECT value FROM play_state WHERE key='playMode'");
    if (modeQuery.next()) {
        // 读取存入的 0, 1, 或 2，并转回枚举类型
        m_currentMode = static_cast<PlayMode>(modeQuery.value(0).toInt());
        // 记得调用你更新图标的函数，让 UI 显示正确的图标
        updateModeUI();
    }
}

void MusicPlayer::savePlaylistToDb()
{
    QSqlDatabase::database().transaction();
    QSqlQuery query;
    query.exec("DELETE FROM playlist"); // 简单重置列表

    query.prepare("INSERT INTO playlist (path, name) VALUES (?, ?)");
    for (int i = 0; i < m_musicFiles.size(); ++i) {
        query.addBindValue(m_musicFiles.at(i));
        query.addBindValue(m_listmodel->item(i)->text());
        query.exec();
    }
    QSqlDatabase::database().commit();
}

void MusicPlayer::closeEvent(QCloseEvent *event)
{
    // 程序关闭前保存状态
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO play_state (key, value) VALUES ('lastIndex', ?)");
    query.addBindValue(m_currentIndex);
    query.exec();
    // 将枚举值（0, 1, 2）转换为整数存入数据库
    query.prepare("INSERT OR REPLACE INTO play_state (key, value) VALUES ('playMode', ?)");
    query.addBindValue(static_cast<int>(m_currentMode));
    query.exec();

    event->accept();
}
/* -------- 打开音乐文件夹 -------- */
void MusicPlayer::on_opendirbtn_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "选择音乐文件夹", "C:/Users/atela/Desktop/qtproject/MusicPlayer/Resource");

    if (dir.isEmpty())
        return;

    m_listmodel->clear();
    m_musicFiles.clear();
    m_currentIndex = -1;

    QDirIterator it(dir, {"*.mp3", "*.wav", "*.flac"}, QDir::Files);
    while (it.hasNext()) {
        QFileInfo info = it.nextFileInfo();
        m_musicFiles << info.absoluteFilePath();
        m_listmodel->appendRow(new QStandardItem(info.fileName()));
    }

    if (m_musicFiles.isEmpty())
        return;

    ui->playbtn->setEnabled(true);
    ui->prevbtn->setEnabled(true);
    ui->nextbtn->setEnabled(true);
    savePlaylistToDb();//保存数据到数据库

    updateSelection(0);
    m_currentIndex = 0;

}
/* -------- 列表激活（双击 / 回车） -------- */
void MusicPlayer::on_MusicList_activated(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    playMusicByIndex(index.row());
}
/* -------- 播放 / 暂停 -------- */
void MusicPlayer::on_playbtn_clicked()
{
    if (m_currentIndex < 0 || m_musicFiles.isEmpty())
        return;

    // 获取播放器当前状态
    QMediaPlayer::PlaybackState state = m_mediaPlayer->playbackState();

    // 1. 如果播放器根本没有加载源 (StoppedState 且 position 为 0)
    // 2. 或者当前的源与我们记录的索引路径不匹配
    bool needsInit = (state == QMediaPlayer::StoppedState && m_mediaPlayer->position() == 0);
    QString curPath = m_mediaPlayer->source().toLocalFile();
    QString targetPath = m_musicFiles.at(m_currentIndex);

    // 使用 QDir::toNativeSeparators 消除系统路径斜杠差异导致的匹配失败
    if (needsInit || QDir::toNativeSeparators(curPath) != QDir::toNativeSeparators(targetPath)) {
        playMusicByIndex(m_currentIndex);
        return; // playMusicByIndex 内部会自动调用 play() 并切换图标
    }

    // --- 正常的播放/暂停切换逻辑 ---
    if (state == QMediaPlayer::PlayingState) {
        m_mediaPlayer->pause();
        // 信号槽会自动切换图标，如果没切，这里手动补一下
        ui->playbtn->setIcon(QIcon(":/Resource/icons8-play-50.png"));
    } else {
        m_mediaPlayer->play();
        ui->playbtn->setIcon(QIcon(":/Resource/icons8-hold-64.png"));
    }
}
/* -------- 下一首 -------- */
void MusicPlayer::on_nextbtn_clicked()
{
    if (m_musicFiles.isEmpty()) return;

    int nextIndex = m_currentIndex;

   if (m_currentMode == RandomPlay) {
        // 随机播放：生成一个不等于当前的随机数
        if (m_musicFiles.size() > 1) {
            do {
                nextIndex = QRandomGenerator::global()->bounded(m_musicFiles.size());
            } while (nextIndex == m_currentIndex);
        }
    }
   else{
        //列表循环和单曲循环手动点击时都会跳到下一曲
       nextIndex=(m_currentIndex+1)%m_musicFiles.size();
    }
    playMusicByIndex(nextIndex); // 执行 QEventLoop 的播放逻辑
}

/* -------- 上一首 -------- */
void MusicPlayer::on_prevbtn_clicked()
{
    if (m_musicFiles.isEmpty()) return;

    int prevIndex = m_currentIndex;

    // 根据当前模式（m_currentMode）计算上一首索引
    switch (m_currentMode) {
    case ListLoop:
    case SingleLoop:
        // 单曲循环和列表循环：向前跳一首，若在开头则跳到最后一首
        prevIndex = (m_currentIndex - 1 + m_musicFiles.size()) % m_musicFiles.size();
        break;

    case RandomPlay:
        // 随机播放：使用 QRandomGenerator 生成一个非当前索引
        if (m_musicFiles.size() > 1) {
            do {
                prevIndex = QRandomGenerator::global()->bounded(m_musicFiles.size());
            } while (prevIndex == m_currentIndex);
        }
        break;
    }
    playMusicByIndex(prevIndex);
}

void MusicPlayer::playMusicByIndex(int index)



{



    if (index < 0 || index >= m_musicFiles.size())



    return;







    m_currentIndex = index;

    QModelIndex modelIndex = ui->MusicList->model()->index(index, 0);

    // 关键：同步更新 UI 列表的选中状态

    ui->MusicList->setCurrentIndex(modelIndex);

    ui->MusicList->scrollTo(modelIndex);



    // Qt 6 稳定切歌流程











    // 获取当前歌曲的完整磁盘路径

    QString songPath = m_musicFiles.at(index);



    // 将后缀名从 .mp3 替换为 .lrc

    QString lyricPath = songPath;

    lyricPath.replace(".mp3", ".lrc", Qt::CaseInsensitive);



    // 调用解析函数（这个函数会把歌词存入 m_lyrics Map中）

    parseLyric(lyricPath);


    // ... 执行播放 ...

    m_mediaPlayer->setSource(QUrl::fromLocalFile(m_musicFiles.at(index)));

    m_mediaPlayer->play();





}



/* -------- UI 选中同步 -------- */
void MusicPlayer::updateSelection(int index)
{
    QModelIndex idx = m_listmodel->index(index, 0);
    ui->MusicList->setCurrentIndex(idx);
    ui->MusicList->scrollTo(idx);
}
/* -------- 播放模式切换 -------- */
void MusicPlayer::on_playMode_clicked()
{
    //切换模式
    m_currentMode=static_cast<PlayMode>((m_currentMode+1)%3);

    // 调用刚才定义的 UI 更新函数
    updateModeUI();
}
void MusicPlayer::updateModeUI()
{
    QString iconPath;
    QString toolTip;

    switch (m_currentMode) {
    case ListLoop:
        iconPath = ":/Resource/icons8-repeat-50.png"; // 确保路径与资源文件一致
        toolTip = "列表循环";
        break;
    case SingleLoop:
        iconPath = ":/Resource/icons8-repeat-one-50.png";
        toolTip = "单曲循环";
        break;
    case RandomPlay:
        iconPath = ":/Resource/icons8-shuffle-50.png";
        toolTip = "随机播放";
        break;
    }

    ui->playMode->setIcon(QIcon(iconPath)); // 更新图标
    ui->playMode->setToolTip(toolTip);      // 更新鼠标悬停提示
}
//歌词显示
void MusicPlayer::parseLyric(const QString &lyricFilePath) {
    m_lyrics.clear();
    QFile file(lyricFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->label->setText("未找到歌词"); // 对应你 page_2 里的 label
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        // LRC格式通常为: [00:12.34]歌词内容
        QRegularExpression re("\\[(\\d+):(\\d+).(\\d+)\\](.*)");
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            int m = match.captured(1).toInt();
            int s = match.captured(2).toInt();
            int ms = match.captured(3).toInt();
            int totalMs = (m * 60 + s) * 1000 + ms;
            m_lyrics.insert(totalMs, match.captured(4).trimmed());
        }
    }
    file.close();
}
void MusicPlayer::on_Lyricsbtn_clicked()
{
    // 如果当前在列表页(0)，则切到歌词页(1)；反之亦然
    int currentIndex = ui->stackedWidget->currentIndex();
    if (currentIndex == 0) {
        ui->stackedWidget->setCurrentIndex(1);
        // 确保封面和歌词是最新的（防止尺寸变化导致显示问题）
        if (ui->albumArtLabel->pixmap().isNull()) {
            loadExternalCover();
        }
    } else {
        ui->stackedWidget->setCurrentIndex(0);
    }
}
//找封面
void MusicPlayer::loadExternalCover() {
    // 1. 安全检查：确保索引有效
    if (m_currentIndex < 0 || m_currentIndex >= m_musicFiles.size()) return;

    // 2. 获取文件夹路径
    QString songPath = m_musicFiles.at(m_currentIndex);
    QString dirPath = QFileInfo(songPath).absolutePath();

    // 3. 定义几种常见的封面文件名
    QStringList coverNames = {"cover.jpg", "cover.png", "folder.jpg", "album.jpg"};

    for (const QString &name : coverNames) {
        QString fullPath = dirPath + "/" + name;
        if (QFile::exists(fullPath)) {
            // --- 关键修改点：直接从文件路径加载 QPixmap ---
            QPixmap pix(fullPath);

            if (!pix.isNull()) {
                // 按照 Label 的当前尺寸进行等比例缩放
                ui->albumArtLabel->setPixmap(pix.scaled(ui->albumArtLabel->size(),
                                                        Qt::KeepAspectRatio,
                                                        Qt::SmoothTransformation));
                return;
            }
        }
    }

    // 4. 如果没找到，显示资源文件里的图标
    // 注意：请确保你的资源文件里确实有这个路径的图片
    QPixmap defaultPix(":/Resource/icons8-lyrics-48.png");
    ui->albumArtLabel->setPixmap(defaultPix.scaled(ui->albumArtLabel->size(),
                                                   Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
}
void MusicPlayer::updateCurrentMusicInfo() {
    if (m_currentIndex < 0 || m_currentIndex >= m_musicFiles.size()) return;

    QString songPath = m_musicFiles.at(m_currentIndex);

    // 1. 加载并解析歌词
    QString lyricPath = songPath;
    lyricPath.replace(".mp3", ".lrc", Qt::CaseInsensitive);
    parseLyric(lyricPath);

    // 2. 如果解析到了歌词，手动显示第一行内容，而不是显示“准备就绪”
    if (!m_lyrics.isEmpty()) {
        // 获取 Map 中第一个时间戳对应的歌词
        ui->label->setText(m_lyrics.first());
    } else {
        ui->label->setText("未找到歌词");
    }

    // 先尝试手动寻找外部封面（因为此时 MediaPlayer 没播放，metaData 可能还未加载）
    loadExternalCover();
}
