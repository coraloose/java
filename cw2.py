# COMP2611-Artificial Intelligence-Coursework#2 - Decision Trees

import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier
from sklearn.metrics import accuracy_score, recall_score
from sklearn.tree import export_text
import warnings
import os

# STUDENT NAME: XXX
# STUDENT EMAIL:  YYY@leeds.ac.uk
    
def print_tree_structure(model, header_list):
    tree_rules = export_text(model, feature_names=header_list[:-1])
    print(tree_rules)
    
# Task 1 [8 marks]: 
def load_data(file_path, delimiter=','):
    if not os.path.isfile(file_path):
        warnings.warn(f"Task 1: Warning - CSV file '{file_path}' does not exist.")
        return None, None, None
    # 读取 CSV 文件
    df = pd.read_csv(file_path, delimiter=delimiter)
    num_rows = df.shape[0]
    data = df.to_numpy()
    header_list = df.columns.tolist()
    return num_rows, data, header_list

# Task 2 [8 marks]: 
def filter_data(data):
    # 去除任一列等于 -99 的行
    filtered_data = data[~(data == -99).any(axis=1)]
    return filtered_data

# Task 3 [8 marks]: 
def statistics_data(data):
    # 先对数据进行过滤
    filtered_data = filter_data(data)
    # 除去最后一列（标签列）
    features = filtered_data[:, :-1].astype(float)
    coefficient_of_variation = []
    for i in range(features.shape[1]):
        col = features[:, i]
        mean_val = np.mean(col)
        std_val = np.std(col)
        if mean_val == 0:
            cv = np.inf
        else:
            cv = std_val / mean_val
        coefficient_of_variation.append(cv)
    return np.array(coefficient_of_variation)

# Task 4 [8 marks]: 
def split_data(data, test_size=0.3, random_state=1):
    # 假设最后一列为标签
    X = data[:, :-1].astype(float)
    y = data[:, -1]
    x_train, x_test, y_train, y_test = train_test_split(
        X, y, test_size=test_size, random_state=random_state, stratify=y
    )
    return x_train, x_test, y_train, y_test

# Task 5 [8 marks]: 
def train_decision_tree(x_train, y_train, ccp_alpha=0):
    model = DecisionTreeClassifier(random_state=1, ccp_alpha=ccp_alpha)
    model.fit(x_train, y_train)
    return model

# Task 6 [8 marks]: 
def make_predictions(model, X_test):
    y_test_predicted = model.predict(X_test)
    return y_test_predicted

# Task 7 [8 marks]: 
def evaluate_model(model, x, y):
    y_pred = make_predictions(model, x)
    accuracy = accuracy_score(y, y_pred)
    recall = recall_score(y, y_pred, zero_division=0)
    return accuracy, recall

# Task 8 [8 marks]: 
def optimal_ccp_alpha(x_train, y_train, x_test, y_test):
    # 先训练未剪枝模型并获得其测试准确率
    unpruned_model = train_decision_tree(x_train, y_train, ccp_alpha=0)
    acc_unpruned, _ = evaluate_model(unpruned_model, x_test, y_test)
    if acc_unpruned == 1.0:
        return 0.0
    last_valid = 0.0
    # 从 0.001 开始，每次增加 0.001，最多 1000 次或直到 ccp_alpha 达到 1.0
    for i in range(1, 1001):
        alpha = i * 0.001
        if alpha > 1.0:
            break
        model_temp = train_decision_tree(x_train, y_train, ccp_alpha=alpha)
        acc_temp, _ = evaluate_model(model_temp, x_test, y_test)
        # 如果准确率下降超过 1%，则返回上一个有效的 ccp_alpha
        if acc_temp < acc_unpruned - 0.01:
            return last_valid
        else:
            last_valid = alpha
    return last_valid

# Task 9 [8 marks]: 
def tree_depths(model):
    depth = model.get_depth()
    return depth

# Task 10 [8 marks]: 
def important_feature(x_train, y_train, header_list):
    last_valid_feature = None
    # 只考虑特征名称（排除标签列）
    features_list = header_list[:-1]
    # 从 ccp_alpha=0 开始，每次增加 0.01，直到决策树深度为 1
    for i in range(0, 101):
        alpha = i * 0.01
        model_temp = train_decision_tree(x_train, y_train, ccp_alpha=alpha)
        d = tree_depths(model_temp)
        if d == 1:
            # 返回根节点分裂所用的特征
            feature_index = model_temp.tree_.feature[0]
            return features_list[feature_index]
        elif d == 0:
            if last_valid_feature is not None:
                return last_valid_feature
            else:
                return None
        else:
            last_valid_feature = features_list[model_temp.tree_.feature[0]]
    return last_valid_feature

# Task 11 [10 marks]: 
def optimal_ccp_alpha_single_feature(x_train, y_train, x_test, y_test, header_list):
    # 先找出最重要特征
    imp_feature = important_feature(x_train, y_train, header_list)
    if imp_feature is None:
        return 0.0
    # 在 header_list[:-1] 中查找该特征的索引
    features_list = header_list[:-1]
    idx = features_list.index(imp_feature)
    # 提取该单一特征（注意保持二维数组形式）
    x_train_single = x_train[:, [idx]]
    x_test_single = x_test[:, [idx]]
    optimal_alpha = optimal_ccp_alpha(x_train_single, y_train, x_test_single, y_test)
    return optimal_alpha

# Task 12 [10 marks]: 
def optimal_depth_two_features(x_train, y_train, x_test, y_test, header_list):
    features_list = header_list[:-1]
    # 找出最重要特征
    first_feature = important_feature(x_train, y_train, header_list)
    if first_feature is None:
        return 0
    # 从特征列表中去除最重要特征，寻找第二重要特征
    remaining_features = [feat for feat in features_list if feat != first_feature]
    # 对应的 x_train 剔除该列
    remaining_indices = [features_list.index(feat) for feat in remaining_features]
    x_train_remaining = x_train[:, remaining_indices]
    # 为 remaining_features 添加标签占位符（以满足函数接口）
    second_feature = important_feature(x_train_remaining, y_train, remaining_features + [header_list[-1]])
    if second_feature is None:
        return 0
    # 在原始数据中获取这两个特征的索引
    idx1 = features_list.index(first_feature)
    idx2 = features_list.index(second_feature)
    # 构建只包含这两个特征的训练与测试数据集
    x_train_two = x_train[:, [idx1, idx2]]
    x_test_two = x_test[:, [idx1, idx2]]
    # 搜索最优的 ccp_alpha
    optimal_alpha_two = optimal_ccp_alpha(x_train_two, y_train, x_test_two, y_test)
    # 训练决策树并返回其深度
    model_two = train_decision_tree(x_train_two, y_train, ccp_alpha=optimal_alpha_two)
    depth = tree_depths(model_two)
    return depth    

# Example usage (Main section):
if __name__ == "__main__":
    # 加载数据
    file_path = "DT.csv"
    num_rows, data, header_list = load_data(file_path)
    print(f"Data is read. Number of Rows: {num_rows}")
    print("-" * 50)

    # 过滤数据
    data_filtered = filter_data(data)
    num_rows_filtered = data_filtered.shape[0]
    print(f"Data is filtered. Number of Rows: {num_rows_filtered}")
    print("-" * 50)

    # 数据统计
    coefficient_of_variation = statistics_data(data_filtered)
    print("Coefficient of Variation for each feature:")
    for header, coef_var in zip(header_list[:-1], coefficient_of_variation):
        print(f"{header}: {coef_var}")
    print("-" * 50)
    
    # 数据拆分
    x_train, x_test, y_train, y_test = split_data(data_filtered)
    print(f"Train set size: {len(x_train)}")
    print(f"Test set size: {len(x_test)}")
    print("-" * 50)
    
    # 训练初始决策树
    model = train_decision_tree(x_train, y_train)
    print("Initial Decision Tree Structure:")
    print_tree_structure(model, header_list)
    print("-" * 50)
    
    # 评估初始模型
    acc_test, recall_test = evaluate_model(model, x_test, y_test)
    print(f"Initial Decision Tree - Test Accuracy: {acc_test:.2%}, Recall: {recall_test:.2%}")
    print("-" * 50)
    
    # 训练修剪后的决策树
    model_pruned = train_decision_tree(x_train, y_train, ccp_alpha=0.002)
    print("Pruned Decision Tree Structure:")
    print_tree_structure(model_pruned, header_list)
    print("-" * 50)
    
    # 评估修剪模型
    acc_test_pruned, recall_test_pruned = evaluate_model(model_pruned, x_test, y_test)
    print(f"Pruned Decision Tree - Test Accuracy: {acc_test_pruned:.2%}, Recall: {recall_test_pruned:.2%}")
    print("-" * 50)
    
    # 搜索最优的 ccp_alpha
    optimal_alpha = optimal_ccp_alpha(x_train, y_train, x_test, y_test)
    print(f"Optimal ccp_alpha for pruning: {optimal_alpha:.4f}")
    print("-" * 50)
    
    # 训练经过修剪与优化的决策树
    model_optimized = train_decision_tree(x_train, y_train, ccp_alpha=optimal_alpha)
    print("Optimized Decision Tree Structure:")
    print_tree_structure(model_optimized, header_list)
    print("-" * 50)
    
    # 获取决策树深度
    depth_initial = tree_depths(model)
    depth_pruned = tree_depths(model_pruned)
    depth_optimized = tree_depths(model_optimized)
    print(f"Initial Decision Tree Depth: {depth_initial}")
    print(f"Pruned Decision Tree Depth: {depth_pruned}")
    print(f"Optimized Decision Tree Depth: {depth_optimized}")
    print("-" * 50)
    
    # 特征重要性
    important_feature_name = important_feature(x_train, y_train, header_list)
    print(f"Important Feature for Fraudulent Transaction Prediction: {important_feature_name}")
    print("-" * 50)
    
    # 单特征下搜索最优 ccp_alpha
    optimal_alpha_single = optimal_ccp_alpha_single_feature(x_train, y_train, x_test, y_test, header_list)
    print(f"Optimal ccp_alpha using single most important feature: {optimal_alpha_single:.4f}")
    print("-" * 50)
    
    # 双特征下搜索最优树深度
    optimal_depth_two = optimal_depth_two_features(x_train, y_train, x_test, y_test, header_list)
    print(f"Optimal tree depth using two most important features: {optimal_depth_two}")
    print("-" * 50)
    
# References: 
# 本代码各任务的实现均基于作业说明中提供的要求和流程。
