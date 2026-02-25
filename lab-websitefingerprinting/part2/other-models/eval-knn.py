import json
import numpy as np

from sklearn.metrics import classification_report
from sklearn.model_selection import train_test_split
from sklearn.neighbors import KNeighborsClassifier


def eval():
    y_pred_full, y_test_full = [], []

    for i in range(10):
        with open("traces.out", "r") as f:
            data = json.loads(f.read())

        X = np.array(data["traces"])
        y = np.array(data["labels"])

        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=0.2, random_state=i, stratify=y
        )

        clf = KNeighborsClassifier(n_neighbors=5)
        clf.fit(X_train, y_train)
        y_pred = clf.predict(X_test)

        y_test_full.extend(y_test)
        y_pred_full.extend(y_pred)

    print(classification_report(y_test_full, y_pred_full))


if __name__ == "__main__":
    eval()
