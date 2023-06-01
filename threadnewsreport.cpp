
#include "threadnewsreport.h"
#include "globalvar.h"

ThreadNewsReport::ThreadNewsReport(QObject *parent)
    : QObject{parent}
{
//    naManager = new QNetworkAccessManager(this);
    tts = new QTextToSpeech(this);
    tts->setLocale(QLocale::Chinese);//设置语言环境
    tts->setRate(0.2);//设置语速-1.0到1.0
    tts->setPitch(0.0);//设置音高-1.0到1.0
    tts->setVolume(0.6);//设置音量0.0-1.0
    id=QDateTime::currentDateTime().addSecs(-1800).toString("yyyyMMddhhmmss");
    jinShiNewsReportCurTime = GlobalVar::settings->value("jinShiNewsReportCurTime").toString();
    eastNewsReportCurTime = GlobalVar::settings->value("eastNewsReportCurTime").toInt();
}

void ThreadNewsReport::getNewsData()
{
    if (isRunning)
        return;
    isRunning=true;
    getEastNews();
    QByteArray allData;
    GlobalVar::getData(allData,1.3,QUrl("https://www.jin10.com/flash_newest.js?t=1667528593473"));
    if (not allData.isEmpty() and not eastNewsList.isEmpty())
        initNewsReport(allData);
//    GlobalVar::timeOutFlag[3]=false;
//    GlobalVar::timeOutFlag[4]=false;
    isRunning=false;
}

void ThreadNewsReport::getEastNews()
{
    QByteArray allData;
    GlobalVar::getData(allData,1.3,QUrl("https://finance.eastmoney.com/yaowen.html"));
    if (allData.isEmpty())
        return;
    QString s=GlobalVar::peelStr(QString(allData),"id=\"artitileList1\"","-1");
    QPair<QString, QString> pair;
    eastNewsList.clear();
    for (int i=1;i<=50;++i)
    {
        QStringList dataList;
        pair=GlobalVar::cutStr(s,"<li id","</li>");
        QString content=GlobalVar::peelStr(pair.first,"<a ","-1");
        s=pair.second;
        QString temp="href=\"";
        dataList<<content.mid(content.indexOf(temp)+temp.length(),GlobalVar::peelStr(content,temp,"-1").indexOf("\""));
        dataList<<GlobalVar::getContent(content);
        dataList<<GlobalVar::getContent(GlobalVar::peelStr(content,"class=\"time\"","-1"));
        eastNewsList.append(dataList);
    }
    std::sort(eastNewsList.begin(),eastNewsList.end(),[](QStringList a,QStringList b){
        return a[2]>b[2];
    });
}

void ThreadNewsReport::initNewsReport(const QByteArray &allData)
{
    QString cur_time=QDateTime::currentDateTime().toString("yyyyMMddhhmmss").mid(10,2);
    if (cur_time <= "01")
    {
        if (count < 3)
        {
            if (tts->state() == QTextToSpeech::Ready)
                tts->say("休息时间,起来锻炼了");
            count+=1;
        }
    }
    else if (("20" <= cur_time and cur_time <= "21") or ("40" <= cur_time and cur_time<= "41"))
    {
        if (count < 3)
        {
            if (tts->state() == QTextToSpeech::Ready)
                tts->say("转转头,伸个懒腰");
            count+=1;
        }
    }
    else
        count = 0;
    QByteArray temp=allData.mid(13,allData.size()-14);
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(temp, &jsonError);

    if (jsonError.error == QJsonParseError::NoError)
    {
        QJsonArray array = doc.array();
        int eastNums=eastNewsList.count()-1;
        int jsNums=array.size()-1;
        while(eastNums!=-1 or jsNums!=-1)
        {
            QString eastTime;
            if (eastNums!=-1)
            {
                QString m=eastNewsList.at(eastNums)[2].split("月")[0];
                QString s=eastNewsList.at(eastNums)[2].split(" ")[1];
//                qDebug()<<m<<s;
                for (int i=0;i<s.size();++i)
                {
                    QString temp=s.mid(i,1);
                    if (GlobalVar::isInt(temp))
                        eastTime+=temp;
                }
                if (m==QDateTime::currentDateTime().toString("M"))
                    eastTime=QDateTime::currentDateTime().toString("MMdd")+eastTime;
                else
                    eastTime=QDateTime::currentDateTime().addDays(-1).toString("MMdd")+eastTime;
//                qDebug()<<QDateTime::currentDateTime().toString("M")<<eastTime;
            }
            else
                eastTime="123456789";
            QString jsTime;
            if (jsNums!=-1)
                jsTime=array[jsNums].toObject().value("id").toString().mid(4,8);
            else
                jsTime="123456789";
            int et=eastTime.toInt();
//            qDebug()<<jsTime.toInt()<<et;
            if (jsTime.toInt()>et and eastNums!=-1)
            {
                sayEastNews(eastNewsList[eastNums],et);
                eastNums-=1;
            }
            else if (jsNums!=-1)
            {
                sayJsNews(array[jsNums].toObject());
                jsNums-=1;
            }
        }
    }
}

void ThreadNewsReport::sayJsNews(QJsonObject object)
{
    QString newId=object.value("id").toString().mid(0,14);
    if (id>=newId)
        return;
    if (object.value("data").toObject().contains("content"))
    {
        if (tts->state() == QTextToSpeech::Ready)
        {
            QString newsText=object.value("data").toObject().value("content").toString();
            if (newsText.contains("<a") or newsText.contains("点击查看") or newsText.contains("金十图示") or
                newsText.contains("＞＞") or newsText.contains("...") or newsText.contains("......") or
                newsText.contains(">>") or newsText.contains("……"))
                return;
            if (newsText=="" or newsText=="-")
                return;
            QString dt=QDateTime::fromString(object.value("time").toString().mid(0,19), "yyyy-MM-ddThh:mm:ss").addSecs(28800).toString("yyyy-MM-dd hh:mm:ss");
            if (jinShiNewsReportCurTime>=dt)
                return;
            if (GlobalVar::isSayNews and tts->state()==QTextToSpeech::Ready)
                tts->say(newsText);
            id=newId;
            emit getNewsFinished("<font size=\"4\" color=red>"+dt+"</font>"+"<font size=\"4\">"+
                                 newsText+"</font>");
            GlobalVar::settings->setValue("jinShiNewsReportCurTime",dt);
        }
    }
}

void ThreadNewsReport::sayEastNews(QStringList l, int time)
{
    if (eastNewsReportCurTime>=time)
        return;
    QString newsText=l[1];
    if (GlobalVar::isSayNews and tts->state()==QTextToSpeech::Ready)
        tts->say("东方财经:"+newsText);
    emit getNewsFinished("<font size=\"4\" color=red>"+l[2]+"</font>"+"<span> <a href="+l[0]+">"+
                         "<font size=\"4\">"+newsText+"</font>"+"</a> </span>");
    eastNewsReportCurTime=time;
    GlobalVar::settings->setValue("eastNewsReportCurTime",time);
}

