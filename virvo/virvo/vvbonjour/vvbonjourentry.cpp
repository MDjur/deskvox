// Virvo - Virtual Reality Volume Rendering
// Copyright (C) 1999-2003 University of Stuttgart, 2004-2005 Brown University
// Contact: Jurgen P. Schulze, jschulze@ucsd.edu
//
// This file is part of Virvo.
//
// Virvo is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library (see license.txt); if not, write to the
// Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

#include "vvbonjourentry.h"

#ifdef HAVE_BONJOUR

vvBonjourEntry::vvBonjourEntry()
{

}

vvBonjourEntry::vvBonjourEntry(const string serviceName,
                               const string registeredType,
                               const string replyDomain)
  : _serviceName(serviceName), _registeredType(registeredType), _replyDomain(replyDomain)
{

}

string vvBonjourEntry::getServiceName() const
{
  return _serviceName;
}

string vvBonjourEntry::getRegisteredType() const
{
  return _registeredType;
}

string vvBonjourEntry::getReplyDomain() const
{
  return _replyDomain;
}

bool vvBonjourEntry::operator==(const vvBonjourEntry& rhs) const
{
  return ((_serviceName == rhs._serviceName)
          && (_registeredType == rhs._registeredType)
          && (_replyDomain == rhs._replyDomain));
}

#endif
// vim: sw=2:expandtab:softtabstop=2:ts=2:cino=\:0g0t0
