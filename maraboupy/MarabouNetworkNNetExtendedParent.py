

'''
/* *******************                                                        */
/*! \file MarabouNetworkNNetExtensions.py
 ** \verbatim
 ** Top contributors (to current version):
 ** Alex Usvyatsov
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief
 ** This class extends MarabouNetworkNNet class
 ** This class is a common parent for other classes extending MarabouNetworkNNet
 ** Has a constructor with all the necessary arguments, but does not implement any functionality
 **
 ** [[ Add lengthier description here ]]
 **/
'''



import MarabouNetworkNNet

class MarabouNetworkNNetExtendedParent(MarabouNetworkNNet.MarabouNetworkNNet):

    def __init__(self, filename="", property_filename = "", perform_sbt=False, compute_ipq = False):
        '''
        Passes the arguments to the parent constructor.
        '''
        super(MarabouNetworkNNetExtendedParent, self).__init__(filename=filename, perform_sbt=perform_sbt)

