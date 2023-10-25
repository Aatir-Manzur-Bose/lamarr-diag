////////////////////////////////////////////////////////////////////////////////
/// @file TimeConvert.h
///
/// Copyright 2002 Bose Corporation, Framingham, MA
////////////////////////////////////////////////////////////////////////////////

#pragma once

#define MS_PER_SEC     1000
#define US_PER_SEC     (MS_PER_SEC * 1000)
#define NS_PER_SEC     (US_PER_SEC * 1000)
#define US_PER_MS      1000
#define NS_PER_US      1000
#define NS_PER_MS      (US_PER_MS * NS_PER_US)
#define NS_TO_MS(x)    ((x) / NS_PER_MS)
#define NS_TO_US(x)    ((x) / NS_PER_US)
#define US_TO_MS(x)    ((x) / US_PER_MS)
#define MS_TO_SEC(x)   ((x) / MS_PER_SEC)
#define SEC_TO_MS(x)   ((x) * MS_PER_SEC)
