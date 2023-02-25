from argparse import ArgumentParser
from paradigm.tools import build
import os

class Assembler(object):
    def initialize(self, paradigm):
        parser = paradigm.initialize(os.path.dirname(os.path.realpath(__file__)))
        return parser
        
    def build(self, paradigm, args):
        p.parse(p.initialize())
        
    def __call__(self):
        p = build.Paradigm()
        parser = self.initialize(p)
        args = p.parse(parser)
        
        generate_cmd = p.generate_command(args)
        build_cmd=""
        if(args.build):
            build_cmd = p.build_command(args)
        
        p.build(False, args.project_dir, generate_cmd, build_cmd)
        
def main():
    assembler = Assembler()
    assembler()

if __name__ == "__main__":main()