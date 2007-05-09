""" example python module to be called by httrack
    
    This module must contain the function "register". This function
    is called by the httrack_py library when httrack's "start" callback
    is executed.
    
    register() must return an instance of a class suitable to execute
    the "python part" of the other callbacks.
    
    NOTE: While Python allows to dynamically add or delete class methods at
    runtime, adding such a method after the call to register() will not
    have any effect, because the corresponding httrack callback is not
    registered and will never be registered. Deleting a method will
    simply lead to a "trivial" execution of the corresponding httrack
    callback.
"""

import os, stat, sys

def register():
    return Htcb()

class Htcb:
    """ example of an httrack callback class. It does not do
        anything useful, except to print some information
    """
    
    def __init__(self):
        self.urls = []
        # just an example how to define error_policy
        # This test/example module may be used from both by the
        # plugin module and by a regular Python program. In the former
        # case, we have REGULAR_STOP, IMMEDIATE_STOP, IGNORE_EXCEPTION
        # available in "this" namespace, in the latter case must import
        # httracklib
        try:
            self.error_policy = {'__default__': REGULAR_STOP,
                                 'start': IMMEDIATE_STOP,
                                 'end': IGNORE_EXCEPTION}
        except NameError:
            import httracklib
            self.error_policy = {'__default__': httracklib.REGULAR_STOP,
                                 'start': httracklib.IMMEDIATE_STOP,
                                 'end': httracklib.IGNORE_EXCEPTION}
        
    
    def __del__(self):
        print "__del__"
        for url in self.urls:
            print "visited", url
    
    def start(self, d):
        """ d contains a dictionary representing almost all members 
            of struct httrackp. This method may change all values,
            but the type of the value should not be changed. Otherwise,
            the value will not be copied back into the httrack data structure
            httrackp. If an element of the dictionary is deleted, it's value 
            is of course also not changed in httrack. Newly created elements
            are ignored. 
            
            At present, the following filed are NOT copied back:
            cookie, exec.
            
            If the return value of this method has a Python boolean value
            "true", the mirroring process is started, otherwise it is aborted.
        """
        print "start", self, d
        return 1
        
    def end(self):
        """ called for httrack callback 'end' 
            For a return value 'false', the mirror will be considered
            aborted, otherwise not.
        """
        print "end"
        return 1
    
    def pause(lockfile):
        """ called for the httrack callback 'pause'. 
            wait until lockfile is deleted 
            Return values are ignored.
        """
        res = stat(lockfile)
        while (stat.S_ISREG(os.stat(lockfile))):
            print "pause for", lockfile
            time.sleep(1)
            
    def query2(self, question):
        """ Called for the httrack callback 'query2'.
            Should return something like 'y', 'yes', 'n', 'no'
            xxx NOT YET TESTED
        """
        print "query2:", question,
        return sys.stdin.readline()
    
    def query3(self, question):
        """ called for the httrack callback 'query3'
            Should return something like '*', '0'..'6'
            xxx NOT YET TESTED
        """
        print "querx3:", question,
        return sys.stdin.readline()
    
    def change_options(self, d):
        """ called for the httrack callback 'change-options'. 
            For the parameter d, see method start 
            If this method returns a 'true' value, the mirror is
            continued, otherwise it is aborted.
        """
        print "change_options", d
        return 1
        
    def check_html(self, html, url_adresse, url_fichier):
        """ called for the httrack callback 'check-html'
            If this method returns a 'true' value, the document is
            further processed, otherwise the document will be skipped.
            If this method raises an exception, the document will be further
            processed.
        """
        print "check_html", url_adresse, url_fichier
        print html
        self.urls.append("http://%s%s" % (url_adresse, url_fichier))
        return 1
    
    def preprocess_html(self, html, url_adresse, url_fichier):
        """ called for the httrack callback 'preprocess-html'
            If this method returns a string, its contents will 
            replace the document data.
            For other return values, the document data is not changed.
            If this method raises an exception, the document data
            is not changed, the mirror will continue nevertheless.
            
            If a realloc error occurs in the httrack-py library, hrmm,
            see the httrack source code for details ;)
        """
        print "preprocess_html", url_adresse, url_fichier
        return "preprocess\n" + html
    
    def postprocess_html(self, html, url_adresse, url_fichier):
        """ called for the httrack callback 'postprocess-html'
            For details, see method precprocess_html
        """
        print "postprocess_html", url_adresse, url_fichier
        return "postprocess\n" + html
    
    def loop(self, lien_back, back_max, back_index, lien_tot, lien_ntot, stat_time):
        """ called for the httrack callback 'loop'
            If the return value is 'true', the mirror is continued, otherwise
            it is aborted.
        """
        # xxx hts_stats_struct stats is yet missing
        print "loop", lien_back, back_max, back_index, lien_tot, lien_ntot, stat_time
        return 1
    
    def check_link(self, adr, fil, status):
        """ called for the httrack callback 'check-link'
            This method should return one of the integers values -1, 0, or 1
            
            1  -> link is accepted
            0  -> link is refused
            -1 -> decision left to httrack
            
            For non-integer retrun values, -1 is assumed
        """
        print "check_link", adr, fil, status
        return -1
    
    def save_file(self, filename):
        """ called for the httrack callback 'save-file'
            called when a file is to be saved on disk
            return values are ignored.
        """
        print "save_file", filename;
    
    def link_detected(self, link):
        """ called for the httrack callback 'link-detected'
            called when has been detected. 
            return value 1 -> link can be analyzed
                         0 -> link must not even be considered
            For non-integer return values, 1 is assumed
        """
        print "link_detected", link
        return 1
    
    def link_detected2(self, link, start_tag):
        """ called for the httrack callback 'link-detected2'
            called when has been detected. 
            return value 1 -> link can be analyzed
                         0 -> link must not even be considered
            For non-integer return values, 1 is assumed
        """
        print "link_detected2", link, start_tag
        return 1
    
    def save_name(self, adr_complete, fil_complete, referer_adr, referer_fil, save):
        """ called for the httrack callback 'save-name'
            If the return value is a string, its value is copied into
            the C string 'save' as described in the httrack API
        """
        print "save_name", adr_complete, fil_complete, referer_adr, referer_fil, save
        
        # just for fun, add some chars to the start of the hostname
        # return "xxx" + save
        # returning nothing is also OK; in this case, the filename is not changed
    
    def send_header(self, buf, adr, fil, referer_adr, referer_fil, incoming):
        """ called for the httrack callback 'send-header'
            If the return value is true, the mirror continues, otherwise
            it is aborted
            
            Possible changes to the dictionary incoming are ignored
        """
        print "send_header", buf, adr, fil, referer_adr, referer_fil
        print "send_header buf", buf
        print "send_header adr", adr
        print "send_header fil", fil
        print "send_header refadr", referer_adr
        print "send_header reffil", referer_fil
        print "incoming", incoming
        return 1
        
    def receive_header(self, buf, adr, fil, referer_adr, referer_fil, incoming):
        """ called for the httrack callback 'receive-header'
            If the return value is true, the mirror continues, otherwise
            it is aborted
            
            Possible changes to the dictionary incoming are ignored
        """
        print "receive_header", buf, adr, fil, referer_adr, referer_fil
        print "receive_header buf", buf
        print "receive_header adr", adr
        print "receive_header fil", fil
        print "receive_header refadr", referer_adr
        print "receive_header reffil", referer_fil
        print "incoming", incoming
        return 1
    
    def transfer_status(self, d):
        print "transfer status", d
        
