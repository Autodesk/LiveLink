# MIT License

# Copyright (c) 2022 Autodesk, Inc.

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import maya.cmds as cmds
import os
import unittest
import logging
from utils import *

class test_rename(unittest.TestCase):
    def setUp(self):
        setUpTest()
        cmds.file(new = True, force = True)
        self.__files = []
        loadPlugins()

    def tearDown(self):
        SubjectPaths = cmds.LiveLinkSubjectPaths()
        if SubjectPaths:
            for Path in SubjectPaths:
                cmds.LiveLinkRemoveSubject(Path)

        cmds.file(new = True, force = True)
        for f in self.__files:
            if os.path.exists(f):
                os.remove(f)
        tearDownTest()

    def ValidateRename(self, defaultName, defaultType, defaultRole, newRole):
        cmds.LiveLinkAddSelection()
        SubjectNames = cmds.LiveLinkSubjectNames()
        SubjectPaths = cmds.LiveLinkSubjectPaths()
        SubjectTypes = cmds.LiveLinkSubjectTypes()
        SubjectRoles = cmds.LiveLinkSubjectRoles()
        self.assertEqual(1, len(SubjectNames))
        self.assertEqual(1, len(SubjectPaths))
        self.assertEqual(1, len(SubjectTypes))
        self.assertEqual(1, len(SubjectRoles))
        self.assertEqual(SubjectNames[0],  defaultName, msg='Unexpected name ' + SubjectNames[0] + 'instead of ' + defaultName)
        self.assertEqual(SubjectTypes[0], defaultType, msg='Type expected to be "' + defaultType + '" but it was "' + SubjectTypes[0] + '" instead')
        self.assertEqual(SubjectRoles[0], defaultRole, msg='Role expected to be "' + defaultRole + '" but it was "' + SubjectRoles[0] + '" instead')
        
        NewName = defaultName + '2'
        print('')
        cmds.LiveLinkChangeSubjectName(SubjectPaths[0], NewName)
        SubjectNames = cmds.LiveLinkSubjectNames()
        SubjectPaths = cmds.LiveLinkSubjectPaths()
        SubjectTypes = cmds.LiveLinkSubjectTypes()
        SubjectRoles = cmds.LiveLinkSubjectRoles()
        self.assertEqual(1, len(SubjectNames))
        self.assertEqual(1, len(SubjectPaths))
        self.assertEqual(1, len(SubjectTypes))
        self.assertEqual(1, len(SubjectRoles))
        self.assertEqual(SubjectNames[0],  NewName, msg='Unexpected name ' + SubjectNames[0] + 'instead of ' + NewName)
        self.assertEqual(SubjectTypes[0], defaultType, msg='Type expected to be "' + defaultType + '" but it was "' + SubjectTypes[0] + '" instead')
        self.assertEqual(SubjectRoles[0], defaultRole, msg='Role expected to be "' + defaultRole + '" but it was "' + SubjectRoles[0] + '" instead')
        
        cmds.LiveLinkChangeSubjectStreamType(SubjectPaths[0], newRole)
        SubjectNames = cmds.LiveLinkSubjectNames()
        SubjectPaths = cmds.LiveLinkSubjectPaths()
        SubjectTypes = cmds.LiveLinkSubjectTypes()
        SubjectRoles = cmds.LiveLinkSubjectRoles()
        self.assertEqual(1, len(SubjectNames))
        self.assertEqual(1, len(SubjectPaths))
        self.assertEqual(1, len(SubjectTypes))
        self.assertEqual(1, len(SubjectRoles))
        self.assertEqual(SubjectNames[0],  NewName, msg='Unexpected name ' + SubjectNames[0] + 'instead of ' + NewName)
        self.assertEqual(SubjectTypes[0], defaultType, msg='Type expected to be "' + defaultType + '" but it was "' + SubjectTypes[0] + '" instead')
        self.assertEqual(SubjectRoles[0], newRole, msg='Role expected to be "' + newRole + '" but it was "' + SubjectRoles[0] + '" instead')

        NewName = defaultName + '3'
        cmds.LiveLinkChangeSubjectName(SubjectPaths[0], NewName)
        SubjectNames = cmds.LiveLinkSubjectNames()
        SubjectPaths = cmds.LiveLinkSubjectPaths()
        SubjectTypes = cmds.LiveLinkSubjectTypes()
        SubjectRoles = cmds.LiveLinkSubjectRoles()
        self.assertEqual(1, len(SubjectNames))
        self.assertEqual(1, len(SubjectPaths))
        self.assertEqual(1, len(SubjectTypes))
        self.assertEqual(1, len(SubjectRoles))
        self.assertEqual(SubjectNames[0],  NewName, msg='Unexpected name ' + SubjectNames[0] + 'instead of ' + NewName)
        self.assertEqual(SubjectTypes[0], defaultType, msg='Type expected to be "' + defaultType + '" but it was "' + SubjectTypes[0] + '" instead')
        self.assertEqual(SubjectRoles[0], newRole, msg='Role expected to be "' + newRole + '" but it was "' + SubjectRoles[0] + '" instead')

        cmds.LiveLinkRemoveSubject(SubjectPaths[0])
        SubjectNames = cmds.LiveLinkSubjectNames()
        SubjectPaths = cmds.LiveLinkSubjectPaths()
        SubjectTypes = cmds.LiveLinkSubjectTypes()
        SubjectRoles = cmds.LiveLinkSubjectRoles()
        self.assertTrue(SubjectNames is None or len(SubjectNames) == 0)
        self.assertTrue(SubjectPaths is None or len(SubjectPaths) == 0)
        self.assertTrue(SubjectTypes is None or len(SubjectTypes) == 0)
        self.assertTrue(SubjectRoles is None or len(SubjectRoles) == 0)

    def test_renameskeletonWithTransfNode(self):
        log = logging.getLogger( "test_rename.test_renameskeletonWithTransfNode" )
        log.info("Started")

        cmds.select( d=True )
    
        self.numberOfNodes = 0

        nameRootSkeleton = 'Root'
    
        NodeNames = [
        nameRootSkeleton,
        'Reference',
        'joint1',
        'joint2'
        ]
    
        # Create a root transform node
        cmds.createNode( 'transform', n=NodeNames[self.numberOfNodes] )
        cmds.move(3,0,0, 'Root', r=True)
        self.numberOfNodes += 1
         
        # Create another transform node
        cmds.createNode( 'transform', n=NodeNames[self.numberOfNodes], p=NodeNames[self.numberOfNodes - 1] )
        cmds.move(0,1,0, 'Reference', r=True)
        self.numberOfNodes += 1
    
        # Create the skeleton joints
        cmds.joint( p=(0,0,0) )
        cmds.rename(NodeNames[self.numberOfNodes])
        cmds.joint( p=(2,0,0) )
        cmds.joint( NodeNames[self.numberOfNodes], e=True, zso=True, oj='xyz', sao='yup' )
        self.numberOfNodes += 1
        
        # joint2 is a child of joint1
        cmds.rename(NodeNames[self.numberOfNodes])
        cmds.joint( NodeNames[self.numberOfNodes], e=True, zso=True, oj='xyz', sao='yup' )
        self.numberOfNodes += 1
        
        # Create a cube baseObject, add subdivision and put it in position
        cmds.polyCube( n='baseObject',w=6, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
        cmds.move(3,0,0, 'baseObject', r=True)
        cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
        
        # Add a skin cluster between the skeleton and baseObject
        cmds.skinCluster('joint1', 'joint2', 'baseObject',tsb=True)
        
        cmds.select(nameRootSkeleton, r=True)
        
        self.ValidateRename(nameRootSkeleton, 'Character', 'Animation', 'Transform')

        # Delete the objects
        cmds.delete(NodeNames)
        cmds.delete('baseObject')

        log.info("Completed")

    def test_renameSpotLight(self):
        log = logging.getLogger( "test_rename.test_renameSpotLight" )
        log.info("Started")

        cmds.select(d=True)
        
        name = 'spotlight'
        cmds.spotLight(n=name, coneAngle=45)
        
        self.ValidateRename(name, 'Light', 'Light', 'Transform')
        
        cmds.delete(name)
        log.info("Completed")
    
    def test_renameCamera(self):
        log = logging.getLogger( "test_rename.test_renameCamera" )
        log.info("Started")

        cmds.select(d=True)

        name = 'camera'
        camera = cmds.camera()
        cmds.rename(camera[0], name)

        self.ValidateRename(name, 'Camera', 'Camera', 'Transform')
        
        cmds.delete(name)
        log.info("Completed")

    def test_renameProp(self):
        log = logging.getLogger( "test_rename.test_renameProp" )
        log.info("Started")

        cmds.select(d=True)

        name = 'prop'
        cmds.polyCube(n=name,w=6, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)

        self.ValidateRename(name, 'Prop', 'Transform', 'Animation')
        
        cmds.delete(name)

        log.info("Completed")
