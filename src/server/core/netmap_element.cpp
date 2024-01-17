/*
** NetXMS - Network Management System
** Copyright (C) 2003-2024 Raden Solutions
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: netmap_element.cpp
**
**/

#include "nxcore.h"
#include <netxms_maps.h>
#include <pugixml.h>

/**************************
 * Network Map Element
 **************************/

/**
 * Generic element default constructor
 */
NetworkMapElement::NetworkMapElement(uint32_t id, uint32_t flags)
{
	m_id = id;
	m_type = MAP_ELEMENT_GENERIC;
	m_posX = 0;
	m_posY = 0;
	m_flags = flags;
}

/**
 * Copy constructor
 */
NetworkMapElement::NetworkMapElement(const NetworkMapElement &src)
{
   m_id = src.m_id;
   m_type = src.m_type;
   m_posX = src.m_posX;
   m_posY = src.m_posY;
   m_flags = src.m_flags;
}

/**
 * Generic element config constructor
 */
NetworkMapElement::NetworkMapElement(uint32_t id, Config *config, uint32_t flags)
{
	m_id = id;
	m_type = config->getValueAsInt(_T("/type"), MAP_ELEMENT_GENERIC);
	m_posX = config->getValueAsInt(_T("/posX"), 0);
	m_posY = config->getValueAsInt(_T("/posY"), 0);
	m_flags = flags;
}

/**
 * Generic element NXCP constructor
 */
NetworkMapElement::NetworkMapElement(const NXCPMessage& msg, uint32_t baseId)
{
	m_id = msg.getFieldAsUInt32(baseId);
	m_type = msg.getFieldAsInt16(baseId + 1);
	m_posX = msg.getFieldAsInt32(baseId + 2);
	m_posY = msg.getFieldAsInt32(baseId + 3);
   m_flags = 0;
}

/**
 * Generic element destructor
 */
NetworkMapElement::~NetworkMapElement()
{
}

/**
 * Update element's persistent configuration
 */
void NetworkMapElement::updateConfig(Config *config)
{
	config->setValue(_T("/type"), m_type);
	config->setValue(_T("/posX"), m_posX);
	config->setValue(_T("/posY"), m_posY);
}

/**
 * Fill NXCP message with element's data
 */
void NetworkMapElement::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
	msg->setField(baseId, m_id);
	msg->setField(baseId + 1, (uint16_t)m_type);
	msg->setField(baseId + 2, (uint32_t)m_posX);
	msg->setField(baseId + 3, (uint32_t)m_posY);
   msg->setField(baseId + 4, m_flags);
}

/**
 * Set element's position
 */
void NetworkMapElement::setPosition(int32_t x, int32_t y)
{
	m_posX = x;
	m_posY = y;
}

/**
 * Update internal fields form previous object
 */
void NetworkMapElement::updateInternalFields(NetworkMapElement *e)
{
   m_flags = e->m_flags;
}

/**
 * Serialize object to JSON
 */
json_t *NetworkMapElement::toJson() const
{
   json_t *root = json_object();
   json_object_set_new(root, "id", json_integer(m_id));
   json_object_set_new(root, "type", json_integer(m_type));
   json_object_set_new(root, "posX", json_integer(m_posX));
   json_object_set_new(root, "posY", json_integer(m_posY));
   json_object_set_new(root, "flags", json_integer(m_flags));
   return root;
}

/**
 * Clone element
 */
NetworkMapElement *NetworkMapElement::clone() const
{
   return new NetworkMapElement(*this);
}

/**********************
 * Network Map Object
 **********************/

/**
 * Object element default constructor
 */
NetworkMapObject::NetworkMapObject(uint32_t id, uint32_t objectId, uint32_t flags) : NetworkMapElement(id, flags)
{
	m_type = MAP_ELEMENT_OBJECT;
	m_objectId = objectId;
	m_width = 100;
	m_height = 100;
}

/**
 * Copy constructor
 */
NetworkMapObject::NetworkMapObject(const NetworkMapObject &src) : NetworkMapElement(src)
{
   m_objectId = src.m_objectId;
   m_width = src.m_width;
   m_height = src.m_height;
}

/**
 * Object element config constructor
 */
NetworkMapObject::NetworkMapObject(uint32_t id, Config *config, uint32_t flags) : NetworkMapElement(id, config, flags)
{
	m_objectId = config->getValueAsUInt(_T("/objectId"), 0);
	m_width = config->getValueAsUInt(_T("/width"), 100);
   m_height = config->getValueAsUInt(_T("/height"), 100);
}

/**
 * Object element NXCP constructor
 */
NetworkMapObject::NetworkMapObject(const NXCPMessage& msg, uint32_t baseId) : NetworkMapElement(msg, baseId)
{
	m_objectId = msg.getFieldAsUInt32(baseId + 10);
	m_width = msg.getFieldAsUInt32(baseId + 11);
   m_height = msg.getFieldAsUInt32(baseId + 12);
}

/**
 * Object element destructor
 */
NetworkMapObject::~NetworkMapObject()
{
}

/**
 * Update element's persistent configuration
 */
void NetworkMapObject::updateConfig(Config *config)
{
	NetworkMapElement::updateConfig(config);
	config->setValue(_T("/objectId"), m_objectId);
	config->setValue(_T("/width"), m_width);
   config->setValue(_T("/height"), m_height);
}

/**
 * Fill NXCP message with element's data
 */
void NetworkMapObject::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
	NetworkMapElement::fillMessage(msg, baseId);
	msg->setField(baseId + 10, m_objectId);
   msg->setField(baseId + 11, m_width);
   msg->setField(baseId + 12, m_height);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapObject::toJson() const
{
   json_t *root = NetworkMapElement::toJson();
   json_object_set_new(root, "objectId", json_integer(m_objectId));
   json_object_set_new(root, "width", json_integer(m_width));
   json_object_set_new(root, "height", json_integer(m_height));
   return root;
}

/**
 * Clone element
 */
NetworkMapElement *NetworkMapObject::clone() const
{
   return new NetworkMapObject(*this);
}

/**************************
 * Network Map Decoration
 **************************/

/**
 * Decoration element default constructor
 */
NetworkMapDecoration::NetworkMapDecoration(uint32_t id, LONG decorationType, uint32_t flags) : NetworkMapElement(id, flags)
{
	m_type = MAP_ELEMENT_DECORATION;
	m_decorationType = decorationType;
	m_color = 0;
	m_title = nullptr;
	m_width = 50;
	m_height = 20;
}

/**
 * Copy constructor
 */
NetworkMapDecoration::NetworkMapDecoration(const NetworkMapDecoration &src) : NetworkMapElement(src)
{
   m_decorationType = src.m_decorationType;
   m_color = src.m_color;
   m_title = MemCopyString(src.m_title);
   m_width = src.m_width;
   m_height = src.m_height;
}

/**
 * Decoration element config constructor
 */
NetworkMapDecoration::NetworkMapDecoration(uint32_t id, Config *config, uint32_t flags) : NetworkMapElement(id, config, flags)
{
	m_decorationType = config->getValueAsInt(_T("/decorationType"), 0);
	m_color = config->getValueAsUInt(_T("/color"), 0);
	m_title = MemCopyString(config->getValue(_T("/title"), _T("")));
	m_width = config->getValueAsInt(_T("/width"), 0);
	m_height = config->getValueAsInt(_T("/height"), 0);
}

/**
 * Decoration element NXCP constructor
 */
NetworkMapDecoration::NetworkMapDecoration(const NXCPMessage& msg, uint32_t baseId) : NetworkMapElement(msg, baseId)
{
	m_decorationType = msg.getFieldAsInt32(baseId + 10);
	m_color = msg.getFieldAsUInt32(baseId + 11);
	m_title = msg.getFieldAsString(baseId + 12);
	m_width = msg.getFieldAsInt32(baseId + 13);
	m_height = msg.getFieldAsInt32(baseId + 14);
}

/**
 * Decoration element destructor
 */
NetworkMapDecoration::~NetworkMapDecoration()
{
	MemFree(m_title);
}

/**
 * Update decoration element's persistent configuration
 */
void NetworkMapDecoration::updateConfig(Config *config)
{
	NetworkMapElement::updateConfig(config);
	config->setValue(_T("/decorationType"), m_decorationType);
	config->setValue(_T("/color"), m_color);
	config->setValue(_T("/title"), CHECK_NULL_EX(m_title));
	config->setValue(_T("/width"), m_width);
	config->setValue(_T("/height"), m_height);
}

/**
 * Fill NXCP message with element's data
 */
void NetworkMapDecoration::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
	NetworkMapElement::fillMessage(msg, baseId);
	msg->setField(baseId + 10, (uint32_t)m_decorationType);
	msg->setField(baseId + 11, m_color);
	msg->setField(baseId + 12, CHECK_NULL_EX(m_title));
	msg->setField(baseId + 13, (uint32_t)m_width);
	msg->setField(baseId + 14, (uint32_t)m_height);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapDecoration::toJson() const
{
   json_t *root = NetworkMapElement::toJson();
   json_object_set_new(root, "decorationType", json_integer(m_decorationType));
   json_object_set_new(root, "color", json_integer(m_color));
   json_object_set_new(root, "title", json_string_t(m_title));
   json_object_set_new(root, "width", json_integer(m_width));
   json_object_set_new(root, "height", json_integer(m_height));
   return root;
}

/**
 * Clone element
 */
NetworkMapElement *NetworkMapDecoration::clone() const
{
   return new NetworkMapDecoration(*this);
}

/**********************************
 * Network Map DCI-based elements
 **********************************/

/**
 * Update container's persistent configuration
 */
void NetworkMapDCIElement::updateConfig(Config *config)
{
   NetworkMapElement::updateConfig(config);
   config->setValue(_T("/DCIList"), m_config);
}

/**
 * Fill NXCP message with container's data
 */
void NetworkMapDCIElement::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
   NetworkMapElement::fillMessage(msg, baseId);
   msg->setField(baseId + 10, m_config);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapDCIElement::toJson() const
{
   json_t *root = NetworkMapElement::toJson();
   json_object_set_new(root, "dciList", json_string_t(m_config));
   return root;
}

/**
 * Update list of referenced DCIs
 */
void NetworkMapDCIElement::updateDciList(CountingHashSet<uint32_t> *dciSet, bool addItems) const
{
   nxlog_debug_tag(_T("netmap"), 7, _T("NetworkMapDCIElement::updateDciList(%u): element configuration: %s"), m_id, m_config.cstr());
   pugi::xml_document xml;
#ifdef UNICODE
   char *xmlSource = UTF8StringFromWideString(m_config);
#else
   char *xmlSource = UTF8StringFromMBString(m_config);
#endif
   if (xml.load_string(xmlSource))
   {
      for (pugi::xpath_node element : xml.select_nodes(getDciXPath()))
      {
         uint32_t id = strtoul(element.attribute().value(), nullptr, 10);
         nxlog_debug_tag(_T("netmap"), 7, _T("NetworkMapDCIElement::updateDciList(%u): found DCI ID %u"), m_id, id);
         if (addItems)
            dciSet->put(id);
         else
            dciSet->remove(id);
      }
   }
   else
   {
      nxlog_debug_tag(_T("netmap"), 6, _T("NetworkMapDCIElement::updateDciList(%u): Failed to load XML"), m_id);
   }
   MemFree(xmlSource);
}

/**
 * Clone DCI container element
 */
NetworkMapElement *NetworkMapDCIContainer::clone() const
{
   return new NetworkMapDCIContainer(*this);
}

/**
 * Get DCI XPath for container element
 */
const char *NetworkMapDCIContainer::getDciXPath() const
{
   return "/config/dciList/dci/@dciId";
}

/**
 * Clone DCI image element
 */
NetworkMapElement *NetworkMapDCIImage::clone() const
{
   return new NetworkMapDCIImage(*this);
}

/**
 * Get DCI XPath for image element
 */
const char *NetworkMapDCIImage::getDciXPath() const
{
   return "/dciImageConfiguration/dci/@dciId";
}


/**************************
 * Network Map Text Box
 **************************/

/**
 * Copy constructor
 */
NetworkMapTextBox::NetworkMapTextBox(const NetworkMapTextBox &src) : NetworkMapElement(src)
{
   m_config = MemCopyString(src.m_config);
}

/**
 * Text Box config constructor
 */
NetworkMapTextBox::NetworkMapTextBox(uint32_t id, Config *config, uint32_t flags) : NetworkMapElement(id, config, flags)
{
   m_config = MemCopyString(config->getValue(_T("/TextBox"), _T("")));
}

/**
 * DCI image NXCP constructor
 */
NetworkMapTextBox::NetworkMapTextBox(const NXCPMessage& msg, uint32_t baseId) : NetworkMapElement(msg, baseId)
{
   m_config = msg.getFieldAsString(baseId + 10);
}

/**
 * DCI image destructor
 */
NetworkMapTextBox::~NetworkMapTextBox()
{
   MemFree(m_config);
}

/**
 * Update image's persistent configuration
 */
void NetworkMapTextBox::updateConfig(Config *config)
{
   NetworkMapElement::updateConfig(config);
   config->setValue(_T("/TextBox"), m_config);
}

/**
 * Fill NXCP message with container's data
 */
void NetworkMapTextBox::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
   NetworkMapElement::fillMessage(msg, baseId);
   msg->setField(baseId + 10, m_config);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapTextBox::toJson() const
{
   json_t *root = NetworkMapElement::toJson();
   json_object_set_new(root, "config", json_string_t(m_config));
   return root;
}

/**
 * Clone element
 */
NetworkMapElement *NetworkMapTextBox::clone() const
{
   return new NetworkMapTextBox(*this);
}
