/*
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
* 
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
* 
* The Original Code is MPEG4IP.
* 
* The Initial Developer of the Original Code is Cisco Systems Inc.
* Portions created by Cisco Systems Inc. are
* Copyright (C) Cisco Systems Inc. 2000, 2001.  All Rights Reserved.
* 
* Contributor(s): 
*              Bill May        wmay@cisco.com
*/

#ifndef __SDP_ERROR_H__
#define __SDP_ERROR_H__

#define ESDP_INVVER  (-10)  /* invalid version*/
#define ESDP_UNKNOWN_LINE  (-11)  /* invalid code*/

#define ESDP_BANDWIDTH (-12) /* invalid bandwidth line*/
#define ESDP_TIME      (-13)
#define ESDP_REPEAT    (-14)
#define ESDP_REPEAT_NOTIME  (-15)
#define ESDP_TIME_ADJ       (-16)
#define ESDP_CONNECT        (-17)
#define ESDP_ORIGIN         (-18)
#define ESDP_MEDIA          (-19)
#define ESDP_KEY            (-20)
#define ESDP_ATTRIBUTES_NO_COLON (-21)

#endif

