import pypsignifit.psigobservers as po
import pypsignifit as pp
import swignifit.interface_methods as  sfi

# script to run a single mcmc

m = 4.0
w = 4.0
l = 0.05
g = 0.02
params = (m, w, l, g)
model = {'nafc':1, 'sigmoid':"logistic", 'core':'mw0.1'}
nblocks = 10
# 128 levels
levels=[3.0,3.03438393847,3.06683976785,3.09760144961,3.1268634863,3.15478934453,3.18151774456,3.20716743383,3.23184086298,3.25562705378,3.27860386336,3.30083979103,3.32239543436,3.34332467315,3.36367564024,3.38349152356,3.40281123349,3.42166996184,3.44009965292,3.45812940282,3.47578579968,3.49309321508,3.51007405486,3.52674897593,3.54313707448,3.55925605012,3.57512234939,3.59075129199,3.60615718186,3.62135340555,3.6363525195,3.6511663277,3.66580595104,3.68028188951,3.69460407794,3.70878193635,3.72282441539,3.73674003753,3.75053693448,3.76422288134,3.77780532778,3.79129142667,3.8046880604,3.81800186517,3.83123925344,3.84440643489,3.85750943587,3.87055411763,3.88354619344,3.89649124479,3.90939473669,3.92226203224,3.93509840657,3.94790906028,3.96069913233,3.97347371267,3.98623785449,3.99899658627,4.01175492376,4.02451788173,4.03729048591,4.05007778479,4.06288486173,4.07571684715,4.08857893112,4.10147637615,4.11441453062,4.12739884254,4.14043487408,4.15352831675,4.16668500741,4.17991094525,4.19321230985,4.20659548044,4.22006705657,4.23363388034,4.24730306039,4.26108199786,4.27497841466,4.28900038424,4.30315636519,4.31745523817,4.33190634649,4.3465195408,4.36130522862,4.37627442923,4.39143883477,4.40681087839,4.42240381053,4.43823178464,4.45430995358,4.4706545787,4.48728315346,4.50421454406,4.52146915008,4.53906908867,4.55703840652,4.57540332489,4.59419252417,4.61343747583,4.63317283161,4.65343688238,4.67427210198,4.69572579606,4.71785088083,4.74070682473,4.76436079535,4.7888890681,4.81437877164,4.8409300716,4.86865893179,4.8977006465,4.92821441759,4.96038937084,4.99445259144,5.0306800504,5.06941176794,5.1110733522,5.15620742767,5.20552095898,5.25995921372,5.3208266688,5.38999588299,5.47029442647,5.56629019787,5.68609831971,5.84638599875,6.09132916932]

priors = ["Gauss(%f,%f)" % (m, m), "Gauss(%f,%f)" % (w, w*2), "Beta(2,50)",
"Beta(1,50)"]


#print 'foo'
#print sfi.mapestimate(data, nafc=model['nafc'], sigmoid=model['sigmoid'], core=model['core'], priors=priors)
#

def print_data(data):
    print "======================================================================"
    for dat in data:
        print dat[0], dat[1], dat[2]


def run():
    ob = po.Observer(*params, **model)
    data = ob.DoAnExperiment(levels, nblocks)
    Bi = pp.BayesInference(data, sigmoid=model['sigmoid'], core=model['core'],
            nafc=model['nafc'], priors=priors, verbose=True)

    ci = Bi.getCI(cut=0.5)
    if ci[0] - ci[2] == 0:
        print "warning, confidence interval has zero length!"
        print_data(data)

for i in range(1000):
    pp.set_seed(i) 
    print "iteration: ", i,
    run()
