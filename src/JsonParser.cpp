/*
 * Copyright (c) 2020 Alex Spataru <https://github.com/alex-spataru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "JsonParser.h"
#include "SerialManager.h"

#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>

/*
 * Only instance of the class
 */
static JsonParser *INSTANCE = nullptr;

/**
 * Shows a macOS-like message box with the given properties
 */
static int NiceMessageBox(QString text, QString informativeText, QString windowTitle = qAppName(),
                          QMessageBox::StandardButtons buttons = QMessageBox::Ok)
{
   auto icon = QPixmap(":/images/icon.png").scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

   QMessageBox box;
   box.setIconPixmap(icon);
   box.setWindowTitle(windowTitle);
   box.setStandardButtons(buttons);
   box.setText("<h3>" + text + "</h3>");
   box.setInformativeText(informativeText);

   return box.exec();
}

/**
 * Initializes the JSON Parser class and connects
 * appropiate SIGNALS/SLOTS
 */
JsonParser::JsonParser()
{
   m_opMode = kAutomatic;
   auto sm = SerialManager::getInstance();
   connect(sm, SIGNAL(packetReceived(QByteArray)), this, SLOT(readData(QByteArray)));
}

/**
 * Returns the only instance of the class
 */
JsonParser *JsonParser::getInstance()
{
   if (!INSTANCE)
      INSTANCE = new JsonParser();

   return INSTANCE;
}

/**
 * Returns the JSON map data from the loaded file as a string
 */
QByteArray JsonParser::jsonMapData() const
{
   return m_jsonMapData;
}

/**
 * Returns the parsed JSON document from the received packet
 */
QJsonDocument JsonParser::document() const
{
   return m_document;
}

/**
 * Returns the file name (e.g. "JsonMap.json") of the loaded JSON
 * map file
 */
QString JsonParser::jsonMapFilename() const
{
   if (m_jsonMap.isOpen())
   {
      auto fileInfo = QFileInfo(m_jsonMap.fileName());
      return fileInfo.fileName();
   }

   return "";
}

/**
 * Returns the file path of the loaded JSON map file
 */
QString JsonParser::jsonMapFilepath() const
{
   if (m_jsonMap.isOpen())
   {
      auto fileInfo = QFileInfo(m_jsonMap.fileName());
      return fileInfo.filePath();
   }

   return "";
}

/**
 * Returns the operation mode
 */
JsonParser::OperationMode JsonParser::operationMode() const
{
   return m_opMode;
}

/**
 * Creates a file dialog & lets the user select the JSON file map
 */
void JsonParser::loadJsonMap()
{
   auto file = QFileDialog::getOpenFileName(Q_NULLPTR, tr("Select JSON map file"), QDir::homePath(),
                                            tr("JSON files (*.json)"));
   if (!file.isEmpty())
      loadJsonMap(file);
}

/**
 * Opens, validates & loads into memory the JSON file in the given @a path.
 */
void JsonParser::loadJsonMap(const QString &path)
{
   // Validate path
   if (path.isEmpty())
      return;

   // Close previous file (if open)
   if (m_jsonMap.isOpen())
   {
      m_jsonMap.close();
      emit jsonFileMapChanged();
   }

   // Try to open the file (read only mode)
   m_jsonMap.setFileName(path);
   if (m_jsonMap.open(QFile::ReadOnly))
   {
      // Read data & validate JSON from file
      QJsonParseError error;
      auto data = m_jsonMap.readAll();
      auto document = QJsonDocument::fromJson(data, &error);
      if (error.error != QJsonParseError::NoError)
      {
         m_jsonMap.close();
         NiceMessageBox(tr("JSON parse error"), error.errorString());
      }

      // JSON contains no errors
      else
      {
         m_jsonMapData = data;
         NiceMessageBox(tr("JSON map file loaded successfully!"),
                        tr("File \"%1\" loaded into memory").arg(jsonMapFilename()));
      }
   }

   // Open error
   else
   {
      NiceMessageBox(tr("Cannot read file contents"),
                     tr("Failed to read contents of \"%1\", check file permissions").arg(jsonMapFilename()));
      m_jsonMap.close();
   }

   // Update UI
   emit jsonFileMapChanged();
}

/**
 * Changes the operation mode of the JSON parser. There are two possible op. modes:
 *
 * @c kManual serial data only contains the comma-separated values, and we need
 *            to use a JSON map file (given by the user) to know what each value means.
 *            This method is recommended when we need to transfer & display a large amount
 *            of information from the microcontroller unit to the computer.
 *
 * @c kAuto serial data contains the JSON data frame, good for simple applications or
 *          for prototyping.
 */
void JsonParser::setOperationMode(const OperationMode mode)
{
   m_opMode = mode;
   emit operationModeChanged();
}

/**
 * Tries to parse the given data as a JSON document according to the selected
 * operation mode.
 *
 * Possible operation modes:
 * - Auto: serial data contains the JSON data frame
 * - Manual: serial data only contains the comma-separated values, and we need
 *           to use a JSON map file (given by the user) to know what each value means
 *
 * If JSON parsing is successfull, then the class shall notify the rest of the
 * application in order to process packet data.
 */
void JsonParser::readData(const QByteArray &data)
{
   if (!data.isEmpty())
   {
      QJsonParseError error;
      auto document = QJsonDocument::fromJson(data, &error);

      if (error.error == QJsonParseError::NoError)
      {
         m_document = document;
         emit packetReceived();
      }
   }
}
