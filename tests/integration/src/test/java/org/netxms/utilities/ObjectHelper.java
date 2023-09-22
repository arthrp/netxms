/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2023 Raden Solutions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.utilities;

import java.io.IOException;

import org.netxms.client.NXCException;
import org.netxms.client.NXCSession;
import org.netxms.client.objects.AbstractObject;
import org.netxms.client.objects.Node;

public class ObjectHelper
{

   /**
    * Find management node
    * 
    * @param session NXCSession object
    * @return management node
    * @throws IOException
    * @throws NXCException
    */
   public static AbstractObject findManagementServer(NXCSession session) throws IOException, NXCException
   {
      session.syncObjects();
      for(AbstractObject object : session.getAllObjects())
      {
         if ((object instanceof Node) && ((Node)object).isManagementServer())
         {
            return object;
         }
      }
      return null;

   }

}
