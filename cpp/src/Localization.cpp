//-----------------------------------------------------------------------------
//
//	Localization.cpp
//
//	Localization for CC and Value Classes
//
//	Copyright (c) 2018 Justin Hammond <justin@dynam.ac>
//
//	SOFTWARE NOTICE AND LICENSE
//
//	This file is part of OpenZWave.
//
//	OpenZWave is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Lesser General Public License as published
//	by the Free Software Foundation, either version 3 of the License,
//	or (at your option) any later version.
//
//	OpenZWave is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Lesser General Public License for more details.
//
//	You should have received a copy of the GNU Lesser General Public License
//	along with OpenZWave.  If not, see <http://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------------------

#include "Localization.h"
#include "tinyxml.h"
#include "Options.h"
#include "platform/Log.h"
#include "value_classes/ValueBitSet.h"

using namespace OpenZWave;

Localization *Localization::m_instance = NULL;
map<int64,ValueLocalizationEntry*> Localization::m_valueLocalizationMap;
map<uint8,LabelLocalizationEntry*> Localization::m_commandClassLocalizationMap;


void LabelLocalizationEntry::AddLabel
(
        string label,
        string lang
        )
{
    if (lang.empty())
        this->m_defaultLabel = label;
    else
        this->m_Label[lang] = label;
}
uint64 LabelLocalizationEntry::GetIdx
(
        )
{
    uint64 key =  ((uint64)m_index << 32) | ((uint64)m_pos);
    return key;

}
string LabelLocalizationEntry::GetLabel
(
        )
{
    return this->m_defaultLabel;
}


uint64 ValueLocalizationEntry::GetIdx
(

        )
{
    uint64 key = ((uint64)m_commandClass << 48) | ((uint64)m_index << 32) | ((uint64)m_pos);
    return key;
}
string ValueLocalizationEntry::GetHelpText
(

        )
{
    return m_DefaultHelpText;
}

void ValueLocalizationEntry::AddHelp
(
        string HelpText,
        string lang
        )
{
    if (lang.empty())
        m_DefaultHelpText = HelpText;
    else
        m_HelpText[lang] = HelpText;

}
string ValueLocalizationEntry::GetLabelText
(
        )
{
    return m_DefaultLabelText;
}
void ValueLocalizationEntry::AddLabel
(
        string Label,
        string lang
        )
{
    if (lang.empty())
        m_DefaultLabelText = Label;
    else
        m_LabelText[lang] = Label;
}



Localization::Localization()
{
}

void Localization::ReadXML
(
        )
{
    // Parse the Z-Wave manufacturer and product XML file.
    string configPath;
    Options::Get()->GetOptionAsString( "ConfigPath", &configPath );

    string path = configPath + "ValueHelp.xml";
    TiXmlDocument* pDoc = new TiXmlDocument();
    if( !pDoc->LoadFile( path.c_str(), TIXML_ENCODING_UTF8 ) )
    {
        delete pDoc;
        Log::Write( LogLevel_Info, "Unable to load ValueHelp file %s", path.c_str() );
        return;
    }

    TiXmlElement const* root = pDoc->RootElement();
    TiXmlElement const* LElement = root->FirstChildElement();
    while( LElement )
    {
        char const* str = LElement->Value();
        char* pStopChar;
        if( str && !strcmp( str, "Localization" ) )
        {
            TiXmlElement const* CCElement = LElement->FirstChildElement();
            while( CCElement )
            {
                str = CCElement->Value();
                if( str && !strcmp( str, "CommandClass" ) )
                {
                    str = CCElement->Attribute( "id" );
                    if( !str )
                    {
                        Log::Write( LogLevel_Warning, "Error in Localization.xml at line %d - missing commandclass ID attribute", CCElement->Row() );
                        CCElement = CCElement->NextSiblingElement();
                        continue;
                    }
                    uint8 ccID = (uint8)strtol( str, &pStopChar, 16 );
                    TiXmlElement const* LabelElement = CCElement->FirstChildElement();
                    while (LabelElement)
                    {
                        str = CCElement->Value();
                        if (str && !strcmp( str, "Label" ) )
                        {
                            ReadXMLLabel(ccID, LabelElement);
                        }
                        if (str && !strcmp( str, "Value" ) )
                        {
                            ReadXMLValue(ccID, LabelElement);
                        }
                        LabelElement = LabelElement->NextSiblingElement();
                    }
                }
            }
        }
        LElement = LElement->NextSiblingElement();
    }
}
void Localization::ReadXMLLabel(uint8 ccID, const TiXmlElement *labelElement) {


    if (m_commandClassLocalizationMap.find(ccID) != m_commandClassLocalizationMap.end()) {
        m_commandClassLocalizationMap[ccID] = new LabelLocalizationEntry(0);
    } else {
        Log::Write( LogLevel_Warning, "Error in Localization.xml at line %d - Duplicate Entry for CommandClass %d", labelElement->Row(), ccID );
    }

    char const* str = labelElement->Attribute( "lang" );

    if( !str )
    {
        m_commandClassLocalizationMap[ccID]->AddLabel(labelElement->GetText());
    }
    else
    {
        m_commandClassLocalizationMap[ccID]->AddLabel(labelElement->GetText(), str);
    }
}

void Localization::ReadXMLValue(uint8 ccID, const TiXmlElement *valueElement) {

    char const* str = valueElement->Attribute( "index");
    if ( !str )
    {
        Log::Write( LogLevel_Info, "Error in Localization.xml at line %d - missing Index  attribute", valueElement->Row() );
        return;
    }
    char* pStopChar;
    uint16 indexId = (uint16)strtol( str, &pStopChar, 16 );

    uint32 pos = -1;
    str = valueElement->Attribute( "pos");
    if (str )
    {
        pos = (uint32)strtol( str, &pStopChar, 16 );
    }

    TiXmlElement const* valueIDElement = valueElement->FirstChildElement();
    while (valueIDElement)
    {
        str = valueIDElement->Value();
        if (str && !strcmp( str, "Label" ) )
        {
            ReadXMLVIDLabel(ccID, indexId, pos, valueIDElement);
        }
        if (str && !strcmp( str, "Help" ) )
        {
            ReadXMLVIDHelp(ccID, indexId, pos, valueIDElement);
        }
        valueIDElement = valueIDElement->NextSiblingElement();
    }
}

void Localization::ReadXMLVIDLabel(uint8 ccID, uint16 indexId, uint32 pos, const TiXmlElement *labelElement) {

    uint64 key = GetValueKey(ccID, indexId, pos);
    if (m_valueLocalizationMap.find(key) != m_valueLocalizationMap.end()) {
        m_valueLocalizationMap[key] = new ValueLocalizationEntry(ccID, indexId, pos);
    } else {
        Log::Write( LogLevel_Warning, "Error in Localization.xml at line %d - Duplicate Entry for ValueID - CC: %d Index: %d Pos: %d", labelElement->Row(), ccID, indexId, pos);
    }

    char const* str = labelElement->Attribute( "lang" );

    if( !str )
    {
        m_valueLocalizationMap[key]->AddLabel(labelElement->GetText());
    }
    else
    {
        m_valueLocalizationMap[key]->AddLabel(labelElement->GetText(), str);
    }
}

void Localization::ReadXMLVIDHelp(uint8 ccID, uint16 indexId, uint32 pos, const TiXmlElement *labelElement) {

    uint64 key = GetValueKey(ccID, indexId, pos);
    if (m_valueLocalizationMap.find(key) != m_valueLocalizationMap.end()) {
        m_valueLocalizationMap[key] = new ValueLocalizationEntry(indexId, pos);
    }

    char const* str = labelElement->Attribute( "lang" );

    if( !str )
    {
        m_valueLocalizationMap[key]->AddHelp(labelElement->GetText());
    }
    else
    {
        m_valueLocalizationMap[key]->AddHelp(labelElement->GetText(), str);
    }
}

uint64 Localization::GetValueKey
(
        uint8 _commandClass,
        uint16 _index,
        uint32 _pos
        )
{
    return ((uint64)_commandClass << 48) | ((uint64)_index << 32) | ((uint64)_pos);
}

void Localization::SetupValue
(
    Value *value
)
{
    uint64 key = GetValueKey(value->GetID().GetCommandClassId(), value->GetID().GetIndex());
    if (m_valueLocalizationMap.find(key) != m_valueLocalizationMap.end()) {
        value->SetHelp(m_valueLocalizationMap[key]->GetHelpText());
        value->SetLabel(m_valueLocalizationMap[key]->GetLabelText());
    } else {
        Log::Write( LogLevel_Warning, "Localization Warning: No Entry for ValueID - CC: %d, Index: %d", value->GetID().GetCommandClassId(), value->GetID().GetIndex());
    }
    /* if its a bitset we need to set the help/label for each bit entry */
    if (value->GetID().GetType() == ValueID::ValueType_BitSet) {
        ValueBitSet *vbs = static_cast<ValueBitSet *>(value);
        uint8 size = vbs->GetSize();
        for (int i = 0; i < size; ++i) {
            key = GetValueKey(value->GetID().GetCommandClassId(), value->GetID().GetIndex(), i);
            if (m_valueLocalizationMap.find(key) != m_valueLocalizationMap.end()) {
                vbs->SetBitHelp(i, m_valueLocalizationMap[key]->GetHelpText());
                vbs->SetBitLabel(i, m_valueLocalizationMap[key]->GetLabelText());
            } else {
                Log::Write( LogLevel_Warning, "Localization Warning: No Entry for ValueID - CC: %d, Index: %d, Pos %d", value->GetID().GetCommandClassId(), value->GetID().GetIndex(), i);
            }
        }
    }
}
void Localization::SetupCommandClass
(
        CommandClass *cc
)
{
    uint8 ccID = cc->GetCommandClassId();
    if (m_commandClassLocalizationMap.find(ccID) != m_commandClassLocalizationMap.end()) {
        cc->SetCommandClassLabel(m_commandClassLocalizationMap[ccID]->GetLabel());
    } else {
        Log::Write( LogLevel_Warning, "Localization Warning: No Entry for CommandClass - CC: %d", ccID);
        cc->SetCommandClassLabel(cc->GetCommandClassName());
    }
}



Localization *Localization::Get
(
        )
{
    if ( m_instance != NULL )
    {
        return m_instance;
    }
    m_instance = new Localization();
    ReadXML();
    return m_instance;
}
