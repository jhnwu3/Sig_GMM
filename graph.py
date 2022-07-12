import matplotlib.pyplot as plt
import numpy as np
import os 
import sys

# def plot_confidence_interval(x, values, z=1.96, color='#2187bb', horizontal_line_width=0.25):
#     mean = np.mean(x)
#     stdev = np.std(x)
#     confidence_interval = z * stdev / np.sqrt(len(values))

#     left = x - horizontal_line_width / 2
#     top = mean - confidence_interval
#     right = x + horizontal_line_width / 2
#     bottom = mean + confidence_interval
#     plt.plot([x, x], [top, bottom], color=color)
#     plt.plot([left, right], [top, top], color=color)
#     plt.plot([left, right], [bottom, bottom], color=color)
#     plt.plot(x, mean, 'o', color='#f44336')

#     return mean, confidence_interval


class Graph_Writer:
    
    graphingDirectory = "/frontend/graph/"
    
    def plot_confidence_interval(x, values, z=1.96, color='#2187bb', horizontal_line_width=0.25):
        mean = np.mean(values)
        stdev = np.std(values)
        print(stdev)
        confidence_interval = z * stdev / np.sqrt(len(values))

        left = x - horizontal_line_width / 2
        top = mean - confidence_interval
        right = x + horizontal_line_width / 2
        bottom = mean + confidence_interval
        plt.plot([x, x], [top, bottom], color=color)
        plt.plot([left, right], [top, top], color=color)
        plt.plot([left, right], [bottom, bottom], color=color)
        plt.plot(x, mean, 'o', color='#f44336')

        return mean, confidence_interval
    
    def getFileName(args):
        return args[args.index('-e') + 1]
    
    def getTime(args):
        return args[args.index('-t') + 1]
    
    def __init__(self, argv):
        self.name = Graph_Writer.getFileName(argv)
        self.t = Graph_Writer.getTime(argv)
        self.estimates = np.genfromtxt(Graph_Writer.graphingDirectory + self.name + '_estimates.csv', delimiter=',')
        print(self.estimates.shape)
        # self.moments = np.genfromtxt()
        
        
    def plotConfidenceIntervals(self, z, simulated = False):
        plt.xticks([1, 2], ['kbirth', 'kdeath'])
        plt.title('Confidence Intervals For t=1, 6 cells, true thetas 0.24, 0.81')
        nRates = self.estimates.shape[1]
        for i in range(nRates):
            Graph_Writer.plot_confidence_interval(i + 1,self.estimates[:,i], z)
        simulated = True
        if simulated:
            plt.plot(1, 0.24, 'D', color='#013220')
            plt.plot(2, 0.81, 'D', color='#013220')
        plt.show()
        plt.savefig(self.name + '_estimates.png')
        
    def plotMoments(self, x, y):
        print("workinprogress")
        return 0
    

   


graph = Graph_Writer(sys.argv)
graph.plotConfidenceIntervals(1.96)