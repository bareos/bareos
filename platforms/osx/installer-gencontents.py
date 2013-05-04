import os
import stat
import sys
from xml.dom.minidom import Document

def createFtags(doc, path, isrootpath=True):
    """
    create f-tags for packagemaker's contents.xml files recursively replacing
    owner with "root" and group with "wheel" in each entry
    """

    statinfo = os.lstat(path)
    isdir = stat.S_ISDIR(statinfo[0])

    ftag = doc.createElement("f")
    ftag.setAttribute("n",os.path.split(path)[1])
    ftag.setAttribute("p","%d" % statinfo[0])
    ftag.setAttribute("o","root")
    ftag.setAttribute("g","wheel")

    # we additionally have to create <mod>owner</mod> and <mod>group</mod>
    # within each f-tag
    ftag.appendChild(doc.createElement("mod").appendChild(doc.createTextNode("owner")))
    ftag.appendChild(doc.createElement("mod").appendChild(doc.createTextNode("group")))

    if isrootpath:
        # needs to be the full path
        ftag.setAttribute("pt",os.path.abspath(path))
        # no idea what those attributes mean:
        ftag.setAttribute("m","false")
        ftag.setAttribute("t","file")

    if isdir:
        for item in os.listdir(path):
            ftag.appendChild(createFtags(doc, os.path.join(path,item), False))

    return ftag

def generateContentsDocument(path):
    """
    create new minidom document and generate contents by recursively traver-
    sing the given path.
    """

    doc = Document()
    root = doc.createElement("pkg-contents")
    root.setAttribute("spec","1.12")
    root.appendChild(createFtags(doc, path))
    doc.appendChild(root)

    return doc

if __name__ == "__main__":
    # construct document
    doc = generateContentsDocument(sys.argv[1])
    print doc.toprettyxml(indent="  "),
