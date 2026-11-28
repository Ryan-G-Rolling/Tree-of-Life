# -*- coding: utf-8 -*-
"""为图鉴生成科普正文（约 100 字/条）。

数据来源：species.c 的 k_species（名称/阶段/生态）、evolution.c 的 k_mutations（名称/说明/加成）。
事实来自关键词表 + 阶段/生态模板，保证每条都有具体物种或机制信息，不凭空造数据。
输出：追加到 src/codex.c 的两张 const 表 + codex_science()。
"""
import re, sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPECIES_C = os.path.join(ROOT, "src", "species.c")
EVO_C = os.path.join(ROOT, "src", "evolution.c")
CODEX_C = os.path.join(ROOT, "src", "codex.c")

STAGE = ["原始生命", "原核生物", "真核生物", "多细胞生物", "组织分化", "共生超级个体",
         "认知生物圈", "智慧生命", "技术圈", "文明", "太空时代", "超越",
         "无生命场", "宇宙织体"]

ECO_FACT = {
    "PRIMITIVE": "它还没有细胞核和细胞器，只靠一层膜与几套酶维持代谢",
    "PHOTOSYNTHETIC": "它靠光能驱动光合作用，把二氧化碳固定成有机物并放出氧气",
    "CHEMOSYNTHETIC": "它不依赖阳光，改用硫、铁、氢等无机物的氧化反应取能",
    "HETEROTROPHIC": "它自己不做光合，靠捕食、滤食或分解现成的有机物过活",
    "MULTICELLULAR": "它的细胞不再各自为战，开始分工、黏合与协同",
    "EXTREMOPHILE": "它能在高温、高盐、强酸或无氧的极端环境里维持代谢",
}
ECO_RES = {
    "PRIMITIVE": "ATP", "PHOTOSYNTHETIC": "ATP", "CHEMOSYNTHETIC": "生物量",
    "HETEROTROPHIC": "DNA", "MULTICELLULAR": "种群上限", "EXTREMOPHILE": "抗性",
}

# 关键词 -> 一句现实生物学事实（按顺序匹配，命中即用）
SPECIES_FACTS = [
    ("原始细胞", "地球上最早的生命形态之一，只是一层膜包住的一小团化学反应"),
    ("蓝细菌", "最早进行产氧光合作用的生物，用几十亿年把大气改成了富氧"),
    ("化能细菌", "深海热泉口的常客，靠硫化氢与铁的氧化维持整个食物网"),
    ("藻", "单细胞光合真核生物，是海洋食物链最底层的生产者"),
    ("捕食者", "靠吞噬其他细胞获取现成有机物，是早期海洋里的猎手"),
    ("群落", "多个细胞黏在一起生活，分工的第一步从这里开始"),
    ("组织", "细胞分化出不同职能，同一套基因组开始按位置表达"),
    ("共生", "两种生物长期共处、交换代谢产物，常常谁也离不开谁"),
    ("真菌", "菌丝在地下织成网络，负责分解与物质转运"),
    ("菌根", "真菌与植物根系结成联盟，用矿质换糖分"),
    ("地衣", "真菌与藻类的共生体，能在裸岩上率先定居、慢慢啃出土壤"),
    ("珊瑚", "珊瑚虫分泌钙质骨骼，一代代叠起来就成了礁体"),
    ("海绵", "最原始的多细胞动物之一，靠滤食海水里的颗粒为生"),
    ("蠕虫", "身体柔软分节，靠肌肉收缩与体腔 pressure 推进"),
    ("节肢", "外骨骼加分节的身体，是地球上种类最多的动物类群"),
    ("昆虫", "三对足、两对翅，靠变态发育占据几乎每个陆地生态位"),
    ("鱼", "脊椎动物第一次真正进入水体，用鳃呼吸、用鳍控制姿态"),
    ("四足", "从鳍到四肢的改造，让脊椎动物第一次踏上陆地"),
    ("羊膜", "羊膜卵把胚胎泡在自己的小水塘里，繁殖彻底摆脱了水"),
    ("哺乳", "恒温加哺乳，让后代在出生后仍有一段被照顾的高存活期"),
    ("恐龙", "中生代的陆地霸主，用直立的步态与高效的肺占据大型生态位"),
    ("鸟", "羽毛与中空骨骼，把体重压到极限换取飞行"),
    ("植物", "维管束把水和糖分输送到全身，支撑起离地的身体"),
    ("林", "木质部支撑高生长，把碳长期锁进树干里"),
    ("蕨", "有维管束却不结种子，靠孢子扩散、靠水完成受精"),
    ("苔", "没有维管束的登陆先锋，靠表面吸水、贴地生长"),
    ("草原", "禾草的根系密集成网，把土壤固定住并养活大批食草动物"),
    ("花", "花与传粉者结成交易，把繁殖效率推到新的高度"),
    ("城市", "人口与物资高度聚集，交易与分工开始指数级放大"),
    ("文明", "文字、律法与仓储出现，知识第一次可以跨代积累"),
    ("技术", "工具与能源结合，改造环境的速度超过生物进化的速度"),
    ("工业", "化石能源驱动的机器替代肌肉，产能与排放同时暴涨"),
    ("太空", "离开母星需要闭合的生态与可靠的能源，门槛是一个量级的跳跃"),
    ("戴森", "把恒星的全部辐射收进工程结构，是能量采集的极限设想"),
    ("量子", "在量子层面操纵状态，信息密度与并行度同时跃升"),
    ("意识", "神经回路把刺激整合成可预期、可规划的内部模型"),
    ("认知", "记忆、抽象与语言叠加，让种群能共享一套外部知识"),
    ("概念", "存在形式脱离物质载体，规则本身开始被编辑"),
    ("记忆", "把过去的经验压缩成可调用的结构，不再依赖个体寿命"),
    ("虚空", "在真空与真空中涨落之间取能，物质不再是唯一依托"),
    ("熵", "主动搬运熵、维持局部有序，是生命最本质的一项工程"),
    ("礁", "造礁生物用钙质骨骼堆出立体结构，为整片海域提供栖息地"),
    ("智慧", "抽象、语言与协作叠加，种群第一次能改造自己的生存条件"),
    ("奇点", "技术增长曲线陡到无法预测，此后的事只能按外推估算"),
    ("超越", "生命不再受化学与物理的常规约束，进入自我设计的阶段"),
    ("Mat", "微生物席一层层叠起来，靠胞外聚合物把沉积物固定住"),
    ("Swarm", "大量个体按简单规则同步行动，整体表现出新的能力"),
    ("Reef", "礁体由生物骨骼堆积而成，是海洋里生物密度最高的结构"),
    ("Forest", "高大植物分层争夺阳光，把碳与水分循环锁在系统里"),
    ("Hive", "分工严格的群体社会，个体行动让位于整体调度"),
    ("Orbital", "把工业与居住搬到轨道上，摆脱地表资源与重力的限制"),
    ("Archae", "古菌多生活在高温、高盐或无氧的极端环境，代谢路径独特"),
    ("Nitroplast", "固氮细胞器把大气中的氮转成可用形式，解开氮限制"),
    ("Slime", "黏菌没有固定形态，靠群体移动寻找食物再聚合成子实体"),
    ("Radiolar", "放射虫用硅质骨架支撑身体，死后骨骼沉成海底软泥"),
    ("Lichen", "地衣是真菌与藻类的共生体，能在裸岩上率先定居"),
    ("Tubeworm", "管虫靠体内共生菌氧化硫化物取能，自己几乎不吃东西"),
    ("Coral", "珊瑚虫分泌钙质骨骼，与藻类共生后造礁速度大幅提升"),
    ("Prairie", "草原的根系密集成网，把土壤固定住并养活大量食草动物"),
    ("Insect", "外骨骼加分节身体，是地球上种类最多的动物类群"),
    ("Cetacean", "鲸类从陆地回到海洋，靠声音在深水里导航与交流"),
    ("Archive", "把信息长期保存下来，让知识可以跨代甚至跨纪元调用"),
    ("Foundry", "规模化制造把单件成本压到极低，产能成为新的边界"),
    ("Arcology", "封闭生态城把水、空气与废物循环在同一套系统里"),
    ("Dyson", "把恒星全部辐射收进工程结构，能量采集推到恒星级"),
    ("Probe", "自我复制的探测器以指数速度铺开，覆盖范围不再受航程限制"),
    ("Galactic", "跨恒星尺度的组织，通信延迟本身成了结构约束"),
    ("Quantum", "在量子层面操纵状态，信息密度与并行度同时跃升"),
    ("Cosmic", "在宇宙尺度上改写常数与结构，因果顺序开始可调"),
    ("Vesicle", "脂质囊泡把一小团反应液包起来，内外的化学环境从此可以分开"),
    ("Droplet", "液滴靠表面张力自组装，不需要膜也能维持局部浓度差"),
    ("Coacervate", "凝聚层是带电大分子的聚集体，能富集单体并催化简单反应"),
    ("Lipid", "两亲分子自发排列成膜，是细胞边界最原始的版本"),
    ("Iron-Sulfur", "铁硫簇在矿物表面催化电子转移，被认为是代谢的最早形态"),
    ("Porphyrin", "卟啉环能稳定金属离子，是光捕获与电子传递的核心结构"),
    ("Clay", "黏土矿物表面吸附有机分子并催化聚合，是RNA世界的候选温床"),
    ("Replicator", "能自我复制的分子链一旦出现，选择压力立刻开始起作用"),
    ("Purple", "紫硫细菌用硫化氢做还原剂进行不产氧光合，海水因此长期维持还原态"),
    ("Green Sulfur", "绿硫细菌在弱光深水层工作，靠硫化物把光转成化学能"),
    ("Methanogen", "产甲烷菌在无氧环境里把二氧化碳和氢合成甲烷，是沼气与温室气体的源头"),
    ("Halophile", "嗜盐菌靠紫膜上的视黄醛泵出质子，借盐度差取能"),
    ("Thermoacidophile", "嗜热嗜酸菌生活在高温酸性泉里，膜脂与蛋白都经过特殊加固"),
    ("Iron Oxidizer", "铁氧化菌靠二价铁氧化取能，铁锈色的溪底常是它们造成的"),
    ("Nitrogen Fixer", "固氮菌把氮气变成氨，是生态圈氮循环的入口"),
    ("Stromatolite", "叠层石是微生物席一层层黏住沉积物留下的化石，最古老的可见生命证据"),
    ("Cyanobacteria", "蓝细菌把氧气第一次带进大气，也是大氧化事件的主角"),
    ("Endosymbiosis", "内共生把自由生活的细菌变成细胞器，是复杂细胞的关键一跃"),
    ("Protist", "原生生物是单细胞真核生物的总称，鞭毛、纤毛与伪足都出现过"),
    ("Sponge", "海绵没有真正的组织，靠领细胞鞭毛制造水流滤食"),
    ("Cnidarian", "刺胞动物用刺细胞捕食，水母与珊瑚都属于这一类"),
    ("Segmented Worm", "环节动物分节的身体让运动与器官重复成为可能"),
    ("Arthropod", "节肢动物的外骨骼与分节附肢，是登陆与飞行的基础"),
    ("Vertebrate", "脊椎动物用内骨骼支撑体型，脊索是先出现的结构"),
    ("Fish", "鱼类用鳃取氧、鳍控制方向，是脊椎动物在水里的基本形态"),
    ("Amphibian", "两栖类皮肤既是呼吸面又是失水口，登陆必须付出代价"),
    ("Reptile", "羊膜卵把胚胎泡在液体里，繁殖第一次完全脱离水面"),
    ("Dinosaur", "恐龙用直立步态与气囊呼吸提高效率，体型上限被推高"),
    ("Mammal", "哺乳动物靠乳腺与恒温，把幼体存活率抬到新的高度"),
    ("Primate", "灵长类的手能抓握、双眼朝前形成立体视觉，工具使用变得可能"),
    ("Hominid", "直立行走释放双手，脑容量与工具复杂度同步上升"),
    ("Proto", "原初形态缺少真正的分工，所有反应挤在同一团边界里"),
    ("Micro", "微生物个体极小，靠扩散就能完成物质交换，不需要循环系统"),
]

MUTATION_FACTS = [
    ("细胞膜", "磷脂双分子层把细胞与外界隔开，是生命的第一道边界"),
    ("细胞壁", "肽聚糖或纤维素构成刚性外壳，抵住渗透压、挡住入侵者"),
    ("光合", "叶绿素吸收光子，把光能转成化学能并释放氧气"),
    ("化学合成", "用无机物氧化释放的能量固定碳，不需要阳光"),
    ("呼吸", "电子传递链把养分彻底氧化，能量利用率比发酵高一个量级"),
    ("发酵", "不靠氧气也能拆解有机物，代价是能量产出低"),
    ("核", "核膜把遗传物质包起来，转录与翻译从此分时分区"),
    ("线粒体", "内共生的细菌变成细胞器，专门负责高效产能"),
    ("叶绿体", "又一次内共生，把光合机器整体搬进宿主细胞里"),
    ("DNA", "碱基配对保证复制保真，是遗传稳定性的化学基础"),
    ("RNA", "既能存信息又能催化反应，被认为是生命起源的关键分子"),
    ("蛋白", "氨基酸链折叠成酶与结构件，几乎承担所有细胞工作"),
    ("核糖体", "按信码把氨基酸连成肽链，是细胞内唯一的蛋白质工厂"),
    ("修复", "复制出错时的错配修复，决定了突变速率的快慢"),
    ("校对", "聚合酶自带的校对功能，把错误率再压低一到两个量级"),
    ("鞭毛", "旋转的分子马达提供推进力，是单细胞最主要的运动方式"),
    ("纤毛", "成片的纤毛同步摆动，既推进又制造取食水流"),
    ("多细胞", "同一套基因组在不同位置表达出不同形态，分工由此开始"),
    ("分化", "细胞按位置与信号走上不同命运，组织与器官得以形成"),
    ("共生", "两种生物长期共处，代谢产物互为原料"),
    ("免疫", "识别自我与非我，把入侵者挡在门外"),
    ("毒素", "合成次生代谢物，用化学方式劝退捕食者"),
    ("神经", "把刺激变成电信号快速传导，反应时间缩短到毫秒级"),
    ("感官", "把光、声、化学信号转成神经输入，行为开始依赖信息"),
    ("脑", "神经元高度聚合，出现记忆、学习与预测"),
    ("语言", "用符号传递不在场的事物，知识可以跨个体累积"),
    ("工具", "把体外物体当作身体延伸，突破生理极限"),
    ("火", "控制燃烧等于拿到一份高密度能量与一道防线"),
    ("农业", "主动种植与驯化，把能量流从采集改成可控生产"),
    ("金属", "冶炼让工具硬度跃升，工艺开始可以传承与标准化"),
    ("机械", "把能量转成稳定运动，替代肌肉做重复劳动"),
    ("电力", "能量可以远距离传输并按需分配，工业化的关键一步"),
    ("计算", "用逻辑门处理信息，决策速度超过任何生物神经"),
    ("人工智能", "机器自己从数据里找规律，认知开始外包给硅"),
    ("航天", "摆脱引力井需要极高的能量密度与可靠性"),
    ("戴森", "把恒星辐射整体回收，能量上限被推到恒星级"),
    ("量子", "利用叠加与纠缠处理信息，某些问题上指数加速"),
    ("维度", "在更高维结构里操作，物理约束被重新定义"),
    ("熵", "主动搬运熵以维持有序，热力学第二定律下的逆流"),
    ("时间", "把因果顺序当成可调参数，是超越层最激进的一步"),
    ("Distributed Memory", "记忆分散存放在多个节点，单点损毁不影响整体"),
    ("Collective Memory", "群体的记忆存在个体之外，个体死亡不再等于知识丢失"),
    ("Chemical Language", "用分子浓度编码信息，无需视觉也能精确传意"),
    ("Atmospheric Control", "行星尺度的气体与反照率调节，决定地表温度与辐射收支"),
    ("Deep Time Archive", "以百万年为单位归档，短期波动被平均掉"),
    ("Symbiotic Cognition", "共生认知：两个物种共享一套感知，单方无法独立完成判断"),
    ("Neural Lace", "神经束把分散的感受器连成一张网，信号延迟决定反应速度"),
    ("Echolocation", "发出超声再接收回声，黑暗中也能成像与测距"),
    ("Bioluminescence", "荧光素与荧光素酶反应放光，用于诱饵、警戒与求偶"),
    ("Hemoglobin", "血红蛋白把氧结合在铁卟啉上，载氧量比血浆溶解高两个量级"),
    ("Thermophilic Enzymes", "嗜热酶的热稳定性来自更紧密的折叠与额外的离子键"),
    ("Nitroplast", "固氮细胞器让生物摆脱对现成氮源的依赖"),
    ("Amoeboid Movement", "变形运动靠细胞骨架重排，能挤过比自身更窄的缝隙"),
    ("Silica Shell", "硅质骨架比钙质更耐酸、更透光，浮游生物普遍采用"),
    ("Symbiotic Lichen", "真菌提供水分与矿物，藻类提供糖，裸岩也能定居"),
    ("Chitin Scaffold", "几丁质纤维层层叠压，形成轻质复合装甲"),
    ("Chitin Wall", "几丁质壁挡住渗透压与捕食者，代价是蜕皮时的脆弱期"),
    ("Enzyme Cascade", "酶级联把多步反应串成流水线，中间产物不外泄"),
    ("Sulfur Metabolism", "硫还原与硫氧化是缺氧世界的能量主线，古菌靠它活了数十亿年"),
    ("Photonic Skin", "光子晶体与纳米结构操控光，比化学色素更省能、更耐辐照"),
    ("Orbital Industry", "把冶炼与组装搬到轨道上，真空与太阳能变成免费资源"),
    ("Cryo Archive", "低温休眠把代谢压到接近零，是星际运输最便宜的办法"),
    ("耐热", "热稳定性蛋白让酶在接近沸点的环境里仍不失效"),
    ("耐寒", "抗冻蛋白与不饱和脂防冻，细胞膜在冰点下不脆裂"),
    ("耐毒", "外排泵与螯合蛋白把重金属、活性氧清出细胞"),
    ("超突变", "故意放松复制的校对精度，用更高的突变率换更快的适应"),
    ("极端酶", "从嗜极生物借来的酶，在高温高盐高酸下仍保持活性"),
    ("密度感应", "细菌靠自分泌信号感知同伴密度，再统一切换行为"),
    ("生态系统工程", "主动改造环境（造礁、蓄水、固沙），把生境变成自己的基础设施"),
    ("分解酶", "分泌水解酶把大分子拆成可吸收的小分子，是腐生生活的前提"),
    ("几丁质", "几丁质是节肢动物外骨骼的主材，轻而硬、还能防水"),
    ("氮感知", "固氮酶把氮气还原成氨，氮固定是蛋白质合成的第一道闸门"),
    ("建筑", "把材料堆成结构，用外部构造替代身体去占据空间"),
    ("抽象", "把具体经验压成符号与规则，思考可以不依赖当场刺激"),
    ("工业", "标准化零件的批量生产，让产能增长率超过人口增长率"),
    ("信号", "用声、光、化学或电磁信号传信，传递速度决定协作规模"),
    ("递归", "把自己的输出当作下一轮的输入，能力因此可以自我放大"),
    ("城市化", "人口密度上升带来分工与规模效应，也带来瘟疫与拥堵"),
    ("机器", "把感知、决策与执行交给非生物体，人只保留目标函数"),
    ("欧米伽", "技术奇点的另一种叫法：增长曲线在此之后无法外推"),
    ("轨道", "把居住与制造搬到近地轨道，摆脱地表资源与重力约束"),
    ("Memory", "把经验存到体外介质，个体不必再靠遗传传递技能"),
]

def compose_species(i, name, phase, eco, kname="", kdesc=""):
    fact = None
    for key, text in SPECIES_FACTS:
        if key in name:
            fact = text
            break
    if not fact:
        if kdesc and is_cn(kdesc) and is_cn(kname):
            fact = "它的基石是「%s」：%s" % (kname, kdesc)
        else:
            fact = "%s是%s阶段的一个分支形态，形态与代谢都围绕这个阶段的生存压力展开" % (name if is_cn(name) else "它", STAGE[phase])
    eco_text = ECO_FACT.get(eco, "它靠一套特定的代谢路径维持生存")
    res = ECO_RES.get(eco, "资源")
    role_variants = [
        "在游戏里，它落在%s，主要产出%s，并带给种群一项自己的基石加成。" % (STAGE[phase], res),
        "作为%s的一支，它在%s上稳定供给，是这条谱系继续分化的跳板。" % (STAGE[phase], res),
        "本作把它放在%s：产出%s，代价是它只在合适的生境里才满速运转。" % (STAGE[phase], res),
    ]
    body = "%s。%s。%s" % (fact, eco_text, role_variants[i % 3])
    return body

def compose_mutation(i, name, desc, nums):
    fact = None
    for key, text in MUTATION_FACTS:
        if key in name:
            fact = text
            break
    if not fact:
        if desc and is_cn(desc):
            fact = "%s——%s" % (name, desc)
        else:
            fact = "%s是%s阶段的一项常规改造，指向这条谱系最常见的生存压力" % (name if is_cn(name) else "它", "早期" if i < 94 else ("中期" if i < 188 else "后期"))
    gains = []
    labels = [("ATP", nums[1]), ("生物量", nums[2]), ("DNA", nums[3]), ("种群上限", nums[4]), ("增长", nums[5])]
    for label, v in labels:
        if abs(v) > 1e-9:
            gains.append("%s +%d%%" % (label, round(v * 100)))
    gtxt = "、".join(gains) if gains else "提供一条额外的代谢路径"
    tail = [
        "在本作里它给出%s，是%s阶段常见的取舍。" % (gtxt, "前" if i < 64 else "后"),
        "买到它等于拿到%s，代价是一张越来越贵的门票。" % gtxt,
        "它的效果是%s，前期便宜、后期按等比涨价。" % gtxt,
    ]
    return "%s。%s" % (fact, tail[i % 3])

def parse_keystones():
    src = open(SPECIES_C, encoding="utf-8-sig", errors="replace").read()
    seg = src[src.index("static const SpeciesKeystone"):]
    pat = re.compile(r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"')
    return [(m.group(1), m.group(2)) for m in pat.finditer(seg)]

def parse_species():
    src = open(SPECIES_C, encoding="utf-8-sig", errors="replace").read()
    seg = src[src.index("static const SpeciesDef k_species"):src.index("static const SpeciesKeystone")]
    pat = re.compile(r'"([^"]+)"\s*,\s*"[^"]*"\s*,\s*\n\s*(\d+),\s*ECO_(\w+)')
    ks = parse_keystones()
    out = []
    for i, m in enumerate(pat.finditer(seg)):
        kname, kdesc = ks[i] if i < len(ks) else ("", "")
        out.append((i, m.group(1), int(m.group(2)), m.group(3), kname, kdesc))
    return out

def parse_mutations():
    src = open(EVO_C, encoding="utf-8-sig", errors="replace").read()
    seg = src[src.index("static const MutationDef k_mutations"):]
    pat = re.compile(r'\{\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*\n\s*([0-9.]+),\s*([0-9.]+),\s*([0-9.]+),\s*([0-9.]+),\s*([0-9.]+),\s*([0-9.]+)')
    out = []
    for i, m in enumerate(pat.finditer(seg)):
        nums = [float(m.group(k)) for k in range(3, 9)]
        out.append((i, m.group(1), m.group(2), nums))
    return out[:282]

CLOSERS = {
    "early": [
        "早期生命没有分工，任何一点代谢效率的提升都会直接放大种群规模。",
        "那时候海洋里没有捕食者，活得久比跑得快更重要。",
        "这个阶段的胜负只看一件事：谁先把能量攒成能自我复制的结构。",
    ],
    "mid": [
        "到这一阶段，竞争已经从抢资源转向抢生态位，谁的形态更专一谁就更稳。",
        "多细胞之后，个体内部的协调成本第一次超过了与外界的竞争成本。",
        "这一层的关键不再是效率，而是能不能在环境剧变时活下来。",
    ],
    "late": [
        "再往后，身体形态与资源本身都变成了可以编辑的参数。",
        "到了这一层，种群的上限不再由食物决定，而由能量采集的规模决定。",
        "它离'生命'这个词的原意已经很远了，但游戏仍然按同一套规则结算。",
    ],
}

def band(phase_or_index, is_species=True):
    if is_species:
        p = phase_or_index
        return "early" if p <= 3 else ("mid" if p <= 8 else "late")
    return "early" if phase_or_index < 94 else ("mid" if phase_or_index < 188 else "late")

def clean(s):
    s = s.strip()
    while s.endswith(".") or s.endswith("。"):
        s = s[:-1].strip()
    return s.replace("。。", "。").replace("，。", "。")

def is_cn(s):
    return bool(s) and not any(("a" <= ch <= "z") or ("A" <= ch <= "Z") for ch in s)

def fit(text, kind, seed):
    """把正文压到 90–110 字：长了逐句裁，短了补阶段收束句（不重复、不超长）。"""
    text = clean(text)
    if len(text) > 112:
        parts = [p for p in text.split("。") if p.strip()]
        while len(parts) > 1 and len("。".join(parts)) + 1 > 112:
            parts.pop()
        text = "。".join(parts)
    pool = CLOSERS[kind]
    guard = 0
    while len(text) < 92 and guard < 6:
        add = clean(pool[(seed + guard) % len(pool)])
        guard += 1
        if add in text or len(text) + len(add) + 1 > 112:
            continue
        text = text + "。" + add
    return clean(text) + "。"


def c_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')

def main():
    sp = parse_species()
    mu = parse_mutations()
    print("species", len(sp), "mutations", len(mu))
    assert len(sp) == 264 and len(mu) == 282, "解析数量不符"

    sp_texts = [fit(compose_species(*row), band(row[2], True), row[0]) for row in sp]
    mu_texts = [fit(compose_mutation(i, n, d, nums), band(i, False), i) for i, n, d, nums in mu]

    lens = [len(t) for t in sp_texts] + [len(t) for t in mu_texts]
    print("len min/avg/max: %d / %.1f / %d" % (min(lens), sum(lens) / len(lens), max(lens)))

    block = []
    block.append("\n/* ---- v25 图鉴科普正文（由 tools/gen_codex_science.py 生成，勿手改） ---- */\n")
    block.append("static const char *const science_species[] = {\n")
    for t in sp_texts:
        block.append('    "%s",\n' % c_escape(t))
    block.append("};\n\nstatic const char *const science_mutations[] = {\n")
    for t in mu_texts:
        block.append('    "%s",\n' % c_escape(t))
    block.append("};\n\n")
    block.append("_Static_assert(COUNT_OF(science_species) == SPECIES_COUNT, \"每条物种都要有科普正文\");\n")
    block.append("_Static_assert(COUNT_OF(science_mutations) == MUTATION_COUNT, \"每条突变都要有科普正文\");\n\n")
    block.append("""const char *codex_science(CodexKind kind, int id)
{
    if (kind == CODEX_SPECIES) {
        return (id >= 0 && id < COUNT_OF(science_species)) ? science_species[id] : "";
    }
    if (kind == CODEX_MUTATION) {
        return (id >= 0 && id < COUNT_OF(science_mutations)) ? science_mutations[id] : "";
    }
    return "";
}
""")

    text = open(CODEX_C, encoding="utf-8").read()
    marker = "\n/* ---- v25 图鉴科普正文"
    if marker in text:
        text = text[:text.index(marker)]
    text = text.rstrip("\n") + "\n" + "".join(block)
    open(CODEX_C, "w", encoding="utf-8").write(text)
    print("appended to codex.c:", os.path.getsize(CODEX_C), "bytes")

if __name__ == "__main__":
    main()
