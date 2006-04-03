#!/usr/bin/env python

from warlock import *
from pywarlock import *
import re

def stow (str):
        match ('You put your ')
        match ('Stow what?')
        put ('stow ' + str)
        matchWait ()

def get (object):
        class EmptyHands (Exception): pass
        condition = True
        while (condition):
                match ('You need a free hand', EmptyHands)
                match ('You get')
                match ('You are already holding that.')
                put ('get my ' + object)
                try:
                        matchWait ()
                        condition = False
                except EmptyHands:
                        stow ('left')
                        stow ('right')

def get_learning_amount (type):
        regex = re.compile ('% (mind lock|very \w+|\w+)')
        exp_queue = Queue.Queue (0)
        def get_experience_handler (str):
                m = regex.search (str)
                if m:
                        removeCallback (get_experience_handler)
                        exp_queue.put (m.group (1))
        mind_regex = re.compile ('Overall state of mind: .*(clear|fluid|murky|tired|frozen|stagnant)', re.IGNORECASE)
        mind_queue = Queue.Queue (0)
        def get_mind_handler (str):
                m = mind_regex.search (str)
                if m:
                        removeCallback (get_mind_handler)
                        mind_queue.put (m.group (1))
        addCallback (get_experience_handler)
        addCallback (get_mind_handler)
        put ('exp ' + type)
        exp = exp_queue.get (True)
        echo ('experience: ' + exp + '\n')
        mind_state = mind_queue.get (True)
        echo ('mind state: ' + mind_state + '\n')
        return (exp, mind_state)
