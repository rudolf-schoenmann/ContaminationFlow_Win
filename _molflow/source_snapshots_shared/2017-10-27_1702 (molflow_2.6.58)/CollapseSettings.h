#pragma once
/*
  File:        CollapseSettings.h
  Description: Collapse settings dialog
  Program:     MolFlow
  Author:      R. KERSEVAN / J-L PONS / M ADY
  Copyright:   E.S.R.F / CERN

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#include "GLApp/GLWindow.h"
class GLWindow;
class GLButton;
class GLTextField;
class GLLabel;
class GLToggle;

//#include "Geometry_shared.h"
//#include "Worker.h"
class Geometry;
class Worker;

class CollapseSettings : public GLWindow {

public:

  // Construction
  CollapseSettings();

  // Component methods
  void SetGeometry(Geometry *s,Worker *w);

  // Implementation
  void ProcessMessage(GLComponent *src,int message);

private:

  Geometry     *geom;
  Worker	   *work;

  GLButton    *goButton;
  GLButton    *goSelectedButton;
  GLButton    *cancelButton;
  GLLabel     *resultLabel;
  GLTextField *vThreshold;
  GLTextField *pThreshold;
  GLTextField *lThreshold;
  GLToggle *l1;
  GLToggle *l2;
  GLToggle *l3;

  bool isRunning;

  size_t nbVertexS;
  size_t nbFacetS;
  size_t nbFacetSS;

};