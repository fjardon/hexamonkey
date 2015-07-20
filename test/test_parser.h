#ifndef TEST_PARSER_H
#define TEST_PARSER_H

#include <QtTest/QtTest>

#include <QObject>
#include <fstream>

#include "gui/qtmodulesetup.h"

class TestParser : public QObject
{
    Q_OBJECT
public:
    TestParser();

private slots:
    void test_asf();

private:
    bool checkFile(const std::string& path, int depth = -1, int width = -1);

    void writeObject(Object& object, const std::string& path, int depth, int width);
    void writeObjectRecursive(Object& object, std::ofstream& file, int currentDepth, int remainingDepth, int width);

    QtModuleSetup moduleSetup;
};

#endif // TEST_PARSER_H