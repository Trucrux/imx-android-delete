/**
 * @file xmltags.h
 *
 *****************************************************************************/
#pragma once

#include <string>
#include <tinyxml2.h>

using namespace tinyxml2;

class XmlTag {
public:
  enum TagType_e {
    TAG_TYPE_INVALID = 0,
    TAG_TYPE_CHAR,
    TAG_TYPE_DOUBLE,
    TAG_TYPE_STRUCT,
    TAG_TYPE_CELL,
    TAG_TYPE_MAX
  };

  XmlTag(const XMLElement *);

  int32_t size();
  const char *value();
  const char *valueToUpper();
  uint32_t valueToUInt();

  TagType_e type();
  bool isType(const TagType_e);

protected:
  const XMLElement *_pElement;
};

class XmlCellTag : public XmlTag {
public:
  XmlCellTag(const XMLElement *);

  int32_t Index();
};
